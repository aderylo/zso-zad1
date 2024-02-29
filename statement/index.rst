.. _z1-elf:


--------------------------------
Zadanie 1: Odtwarzanie relokacji
--------------------------------

Data ogłoszenia: 27.02.2024

Termin oddania: 26.03.2024 (ostateczny 09.04.2024)

.. contents::

.. toctree::
   :hidden:


Materiały dodatkowe
-------------------

TBA

Przykładowe pliki przed usunięciem symboli:

- :download:`a.elf`
- :download:`b.elf`

Skrypt linkera: :download:`picolibc.ld`.


Wprowadzenie
------------

Jak mówi porzekadło: "żaden program nie jest wolny od błędów".
W dzisiejszych czasach, gdy oprogramowanie otacza nas ze wszystkich stron, istnieje potrzeba radzenia sobie z błędami odkrytymi po latach [#hp]_.
W wielu takich przypadkach często kod źródłowy nie jest dostępny, a producenta na próżno szukać.
Zlokalizowanie problemu i naprawienie go wymaga od nas analizy i zmiany na poziomie kodu binarnego [#tsue]_ [DdisasmUSENIX]_.

Dekompilacja kodu maszynowego do języka "wysokopoziomowego" jak C jest procesem skomplikowanym, niejednoznacznym i nierozstrzygalnym w ogólnym przypadku.
Celem rekompilacji jest ponowna kompilacja zdekompilowanego kodu tak, aby w pełni zachować oryginalną funkcjonalność.
Idealnie, oczekiwalibyśmy, aby wynikowy kod maszynowy był identyczny z kodem dekompilowanym (*roundtrip recompilation*).

W praktyce, aby zmienić działanie programu, nie jest potrzebna niepewna rekompilacja:
po zlokalizowaniu problemu często możemy po prostu nałożyć łatę, w której pierwsze bajty
problematycznej procedury podmieniamy na skok do nowego kodu umieszczonego w wolnej przestrzeni.
W poście_ na swoim blogu Gynvael Coldwind argumentuje, iż ze względu na niepraktyczność rekompilacji zmodyfikowanego kodu,
możemy z dużo dozą pewności stwierdzić czy oprogramowanie było modyfikowane na poziomie binarnym.

Celem niniejszego zadania jest doświadczenie tego problemu "na własnej skórze".

Problem
-------

Podstawowym problemem jest pełna deasemblacja, tj. taka, która pozwoli kod asemblera dowolnie lokalnie modyfikować, a następnie ponownie zasemblować.

Podczas gdy trywialnym jest zamienić bajty na odpowiadający im mnemonik asemblera,
nie jest oczywiste **które** bajty.
Jest to problem **wykrycia granic instrukcji**, szczególnie istotny dla architektur ze zmienną ich długością jak x86.
Jeśli zaczniemy dekodować ciąg instrukcji w złym miejscu, wyjdzie zupełnie inny kod.
Co więcej, możemy pomylić kod z danymi (*content classification*).


.. code:: objdump

  # objdump a.elf -d --start=0x08049072 --stop=0x8049080
  08049072 <foo>:
  8049072:	55                   	push   %ebp
  8049073:	89 e5                	mov    %esp,%ebp
  8049075:	83 ec 08             	sub    $0x8,%esp
  8049078:	e8 dd 00 00 00       	call   804915a

  # objdump a.elf -d --start=0x08049074 --stop=0x8049082
  8049074 <foo+0x2>:
  8049074:	e5 83                	in     $0x83,%eax
  8049076:	ec                   	in     (%dx),%al
  8049077:	08 e8                	or     %ch,%al
  8049079:	dd 00                	fldl   (%eax)
  804907b:	00 00                	add    %al,(%eax)
  804907d:	05 83 0f 00 00       	add    $0xf83,%eax

Drugą kwestią jest **symbolizacja**: chcemy, aby elementy odnoszące się do adresów w kodzie
nie były z nimi związane literalnie, a symbolicznie.
W przeciwnym przypadku, gdybyśmy chcieli dołożyć w pewnym miejscu kilka instrukcji,
adresy dalszego kodu by się pozmieniały, a więc odniesienia do nich byłyby nieaktualne [#gynvael]_.
Oczywiście, nie jesteśmy w stanie poznać oryginalnych nazw etykiet.
Co więcej, ponownie możemy pomylić adres (np. w jump table) z danymi (*Literal Reference Disambiguation* [DdisasmUSENIX]_).

Zadanie
-------

Esencję ww. problemów wyrazimy jako odtworzenie tablicy relokacji wraz z podziałem obszarów na funkcje i zmienne globalne
w pliku ELF maksymalnie pozbawionym symboli (``strip``).
W zadaniu należy napisać program, który dostanie na wejściu plik ELF typu ``ET_EXEC`` zawierający jedynie nagłówki programu (z zawartością segmentów).
Program ma stworzyć nowy plik ELF typu ``ET_REL`` zawierający sekcje:

- ``SHT_REL`` z informacjami o odtworzonych relokacjach (zmieniając też zawartość segmentów).
- ``SHT_SYMTAB``/``SHT_STRTAB`` zawierające "zmyślone" symbole: nazwa symbolu powinna być postaci ``x<address in 08x format><type>``. ``<type>`` to jednoliterowy kod jak podawany przez ``nm``. Np. ``x08049072f``.
- dla każdej z odtworzonych funkcji sekcję ``SHF_PROGBITS`` o nazwie ``.text.<symbol_name>``.
- dla każdej odtworzonej zmiennej sekcję ``SHF_PROGBITS``/``SHF_NOBITS`` o nazwie ``.{text,rodata,data,bss,...}.<symbol_name>`` odpowiednio do flag segmentu.

Za realizację uproszczonej wersji zadania będzie można otrzymać część punktów.

Sukces odtworzenia relokacji będziemy mierzyć poprzez porównanie zbioru par ``(adres, typ)`` w sekcjach ``SHT_REL`` między wejściem (przed usunięciem) a wyjściem.
Poprawne odtworzenie pliku relokowalnego sprawdzimy poprzez przepuszczenie go przez linker i porównanie z plikiem wejściowym.
Natomiast prawdziwym testem będzie podmiana funkcji (np. za pomocą ``objcopy --update-section .text.x08049072f=new_foo.bin``), zlinkowanie i sprawdzenie poprawnego wykonania.

Założenia
---------

Platforma
^^^^^^^^^


Aby być wierni wprowadzeniu, dobrze byłoby, gdyby owo zadanie dotyczyło kodu wykonywanego na mikrokontrolerze [#mcus]_.
Z drugiej strony zadanie powinno dotyczyć znanej z innych zajęć architektury...
Okazuje się, że Intel w 2015 roku wypuścił (krótko żywy) mikrokontroler oparty na architekturze i386: `Intel Quark`_ [#quark]_.
Procesor oparty jest na architekturze Pentium Pro (i586), ale wprowadza nowe ABI IA-MCU_, które ogranicza zaszłości historyczne serii jako opcjonalne (tryb 16-bitowy, rejestry segmentów).
Ponadto ów mikrokontroler nie ma pamięci wirtualnej, a więc przestrzeń adresów jest z góry znana.

W celach testowania rozwiązań będziemy "udawać" kompilację na Intel® Quark™ SE C1000, który udostępnia aplikacji 192KiB pamięci Flash (tylko do odczytu) oraz 40 KiB RAM.
"Udawać", ponieważ ostatecznie skompilowany program będzie zwykłym plikiem wykonywalnym na Linuksie.
W skrócie polega to na kompilacji z flagami i skryptem linkera charakterystycznymi dla tej platformy,
ale zamiast sterowników urządzeń wstawiamy proste wywołania systemowe (``int 0x80``) i dostosujemy startup (``_start``).
Nawet jeśli wydaje się to nieco skomplikowane, to ostatecznym celem jest uproszczenie i ukonkretnienie zakresu zadania.


Wytyczne
^^^^^^^^


.. _IA-MCU:

1.  ABI IA-MCU wprowadza kilka zmian względem ABI System V (używane domyślnie przez Linuksa).
    Szczegóły można znaleźć w dokumencie `iamcu psABI`_.
    Najważniejszą różnicą jest przekazywanie trzech pierwszych argumentów przez rejestry.

    - Możemy założyć Intel Pentium ISA bez jednostki zmiennoprzecinkowej x87.
    - Nie ma rejestrów typu zmiennoprzecinkowego ani wektorowego.
    - Możemy założyć, że nie ma (bo są opcjonalne) rejestrów segmentowych, relokacji TLS, wektora pomocniczego *auxiliary vector*
      i trybu 16-bitowego (a więc też instrukcji *far jump* itp.)
    - Typy skalarne szersze niż 4 bajty są wyrównane.
    - Stos jest zawsze wyrównany do 4 bajtów.
    - ``long double`` to zwykły ``double``
    - Argumenty są przekazywane w rejestrach *eax*, *edx* i *ecx* (*scratch registers*).
      Jeśli potrzeba 8 bajtów, to używane są dwa kolejne.
      Pozostałe argumenty przekazywane na stosie.
    - Powyższe dotyczy też typów złożonych w tym struktury, float i double.
    - Funkcje ze zmienną liczbą argumentów mają je przekazywane w całości na stosie.
    - Wynik zwracany jest w *eax*. Jeśli jest 8-bajtowy, to górne bajty dodatkowo w *edx*.
    - Reszta tak samo, jak w System V ABI. Szczegóły w  `iamcu psABI`_.


2.  Wejściem do procesu odtwarzania jest zawsze jeden plik binarny w formacie ``ET_EXEC`` o architekturze ``EM_IAMCU`` (może być użyte ``EM_386``). Można założyć, że programy kompilowane/asemblowane są gcc/binutils z Debiana 11 (wersja 10). Co więcej,

    - Kompilowano pod Intel Quark z ABI IA-MCU (flagi ``-march=lakemont -mtune=lakemont -m32 -miamcu``).
    - Bez biblioteki standardowej (flagi ``-nostdlib -fno-builtin -mno-default``), ale z osobną wersją picolibc_.
    - Z podziałem na oddzielne sekcje każdej funkcji i danych (``-ffunction-sections -fdata-sections``) oraz nie ma funkcji bez odwołań (``-Wl,--gc-sections``)
    - Nie było sekcji ``.eh_frame`` odwijania stosu ani obsługi wyjątków (flagi ``-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions``).
    - Mogło być wiele plików pośrednich ``.o`` (``ELF_REL``) i mogły być wcześniej zlinkowane częściowo (``-r``).
    - Wzorcowy plik wykonywalny zawiera sekcję relokacji (``-Wl,--emit-relocs``).
    - Linkowanie jest statyczne (``-static``), choć nie wyklucza to relokacji w asemblerze.
    - Plik wejściowy ma usunięte wszystkie metadane poza segmentami (``objcopy --strip-all --strip-section-headers``/``llvm-objcopy --strip-all --strip-sections``).
    - Proste testy są kompilowane z ``-O0``, ale trudniejsze mogą mieć dodatkowe flagi optymalizacji/generowania kodu (``-O*, -f*``).
    - Szczegóły w archiwum/repozytorium z testami (TBA).

3.  Można założyć, że nie będzie innych typów symboli/sekcji/relokacji niż w testach.

    -  W ramach rosnącej trudności, relokacje:

       - ``R_386_32``, ``R_386_PC32``,
       - ``R_386_GOTOFF``, ``R_386_GOTPC``, ``R_386_GOT32``,
       - ``R_386_PLT32``
       - ``R_386_JMP_SLOT``, ``R_386_GLOB_DAT``, ``R_386_RELATIVE`` ? (TBA)
       - ``R_386_SIZE32`` ? (TBA)

4. Utworzone pliki relokowalne będą linkowane komendą zawierającą co najmniej flagi:
   ``cc -m32 -miamcu -nostdlib -Wl,--emit-relocs -Wl,--build-id=none -Wl,--gc-sections -T linker_script.ld # TBA: -static ?``

5. W uproszczonej wersji zadania, a więc z mniejszą liczbą punktów do zdobycia (TBA), plik wejściowy zachowa:

   a) (max TBA ~8): nagłówki sekcji (.text, .rodata, .data.rel.ro, .got, .data, .bss,  .heap, .stack, itp.),
   b) (max TBA ~6-7): zachowa symbole z adresami funkcji (bez rozmiaru),
   c) (max TBA ~4-5): zachowa wszystkie symbole.
   d) Wersja minimum zaliczenia (max. 3) wymaga jedynie odtworzenia ``ET_REL`` z ``ET_EXEC`` mając dostęp do wszystkich symboli i wykonanych relokacji.


6. Można zdobyć dodatkowy punkt za operowanie na formie odpowiadającej obrazowi we Flashu Quarka, tj. po przepuszczeniu przez
   ``objcopy -O binary --gap-fill 0x90``. [#startup]_


Odzyskiwanie relokacji
----------------------

Z natury problemu oczekujemy, że rozwiązania będą heurystyczne.
Innymi słowy, celem jest działanie na przykładowych programach z testów bez nadmiernego się do nich dopasowania.
W praktyce powinno to oznaczać, że z dużym prawdopodobieństwem inne tego typu programy byłyby poprawnie odtworzone.
Ponadto naszym celem nie są programy celowo zaciemniane.
W tej sekcji opiszemy *przykładowe* podejście do rozwiązywania zadania.

Możemy wyróżnić wzajemnie zależne podzadania:

- znaleźć potencjalne granice funkcji,
- znaleźć potencjalne granice instrukcji,
- znaleźć potencjalne wywołania funkcji,
- znaleźć potencjalne referencje do funkcji i danych,
- znaleźć miejsca, które mogły być relokowane.

oraz modyfikację pliku ELF:

- dodanie sekcji i symboli,
- dodanie relokacji i odpowiednia do nich zmiana zawartości segmentów.

.. pull-quote::
    Na pewno warto zacząć od adresu startowego w pliku ELF.
    Musimy jednak pamiętać, że nie jest to typowa funkcja.

Idąc śladem [DdisasmUSENIX]_, możemy na przykład:

1. Zacząć od próby zdekodowania instrukcji zaczynając w każdym bajcie tworząc tablicę ``instr[offset]``.
2. Ustalić flagę (predykat) czy dany offset może być początkiem instrukcji ``is_instr[offset]``:

   - Niepoprawne kodowania lub kodowania instrukcji poza zakresem architektury/typu kodu nie mogą.
   - Jeśli instrukcja może kontynuować wykonanie (tj. nie jest bezwarunkowym skokiem) do niepoprawnej instrukcji, to ona też nie może być poprawna. (reguły 1, 2, 3 i 4 z publikacji).
   - Instrukcja nie może wykonywać skoku do niepoprawnej instrukcji ani poza obszar pamięci.
   - Kod musi być zgodny z ABI (np. nie może używać rejestru, który nie jest parametrem, zanim go nie ustawi/zrzuci do pamięci; sekcja 5.1 publikacji).
   - I tak dalej wedle własnych pomysłów...

3. Heurystycznie znaleźć granice funkcji (np. przypisując adresom jakąś ocenę):

   - Nie ma nieużywanych funkcji, a co za tym idzie:
   - Każda funkcja musi być celem wywołania ``CALL`` albo tail-chainingu ``JUMP``, potencjalnie z rejestru.
   - Bezwzględne adresy funkcji mogą sugerować *jump table* albo tablice używane do łączenia modułów (PLT).
   - Bez częściowego inline'ingu funkcje zazwyczaj nie skaczą do środka innych funkcji.
   - itd.

4. Wykryć idiomatyczne fragmenty kodu (np. dostęp do ``GOT``).

5. Heurystycznie sklasyfikować dane (sekcja 6.1 publikacji):

   - Czy jest dostęp z kodu pod dany adres?
   - Czy obecny adres występuje literalnie?
   - Czy dane są ciągiem poprawnych adresów?
   - Czy dane wyglądają jak ciąg znaków (drukowalne ASCII/UTF-8) zakończony zerem?
   - Czy wyrównanie jest zgodne z ABI?
   - itd.

6. Opracować, które miejsca potrzebują relokacji.

   - Nie relokujemy względnych skoków w ramach jednej funkcji.

7. Uzupełnić plik ELF o podział na sekcje, dodać symbole i relokacje.

   - Należy pamiętać, że kolejność sekcji ma znaczenie podczas linkowania.

Podsumowując, celem nie jest implementacja konkretnej publikacji (tej podlinkowanej czy jakiejkolwiek innej),
a własne kreatywne podejście do problemu (być może się nimi inspirując).

Ocenianie
---------

Każdy program testowy jest automatycznie oceniany w trzech krokach:

1. Bazowym wynikiem jest zgodność symbolizacji.
   Użyta jest metryka **IoU** (Intersection over Union) relokacji (par adres+rodzaj).
   Oznacza to, że wyznaczymy zbiór relokacji ``student_relocs`` z pliku wyjściowego oraz ``true_relocs`` z pliku wejściowego.
   **IoU** oznacza moc części wspólnej tych zbiorów podzieloną przez ich sumę.
   W szczególności, równe zbiory mają ``IoU==1``.
   Typy relokacji, które mogły być odtworzone niejednoznacznie, uznamy za równoważne.
   Relokacje danych (``.rodata, .data`` itp.) mogą mieć mniejszą wagę (TBA).
2. Roundtrip relinkowanie: po symbolizacji i przepuszczeniu przez linker, muszą wyjść identyczne segmenty.
   Ponieważ relinkowanie może zadziałać poprawnie nawet dla niepoprawnej symbolizacji, błędne relinkowanie jest surowo ocenianie przez podzielenie wyniku przez 3.
3. Relinkowanie z podmienioną funkcją na krótszą/dłuższą: błąd dzieli wynik przez 2.

Łącznie, testy umożliwiają zdobycie 0-15 punktów (acz 15 może być trudne).
Dodatkowo, review kodu może odjąć dowolną liczbę punktów [#tests]_, co daje w wyniku ocenę: ``grade_base``,
która jest obcięta do przedziału [0, 10].
Dopiero po obcięciu obliczane są kary za spóźnienie wedle wzoru.

W pseudokodzie (``**`` oznacza potęgowanie)::

    eval1(test) = len(set(student_relocs) & set(true_relocs)) / len(set(student_relocs) | set(true_relocs))
    score(test) = eval1(test) / (check2(test) ? 1 : 3) / (check3(test) ? 1 : 2)
    tests_score = sum(score(t) for t in public_tests)
    limit = 10 # or less for a simplified task
    grade_base = clamp(tests_score - review_issues, 0, 10) * limit / 10
    grade_usos = grade_base * 0.9 ** max(0, delay_days - premium_days)




Rozwiązania
-----------

Rozwiązanie może być wykonane w dowolnej, nieegzotycznej technologii.
W uproszczeniu oznacza to TOP20 popularnych języków (z rankingów TIOBE_ bądź JetBrains_) oraz języki uczone na obowiązkowych/obieralnych stałych przedmiotach na MIM.

Rozwiązanie powinno być odpowiednio udokumentowane, aby czytelnik zrozumiał cele i założenia fragmentów oraz nie wymagało to biegłości w języku (np. dot. "magicznych" operatorów).
W szczególności proszę załączyć plik **README** z krótkim opisem rozwiązania, użytymi zewnętrznymi dependencjami i ewentualnym komentarzem.

Oczywiście, należy zadanie rozwiązywać samodzielnie i nie korzystać z bardziej zaawansowanych narzędzi analizy binarek niż biblioteki do parsowania/edytowania ELF-ów, deasemblacji, powtórnej asemblacji czy gcc/binutils.
W szczególności oznacza to absolutne wykluczenie modułów analizujących z narzędzi wspierających reverse engineering jak Radare czy Ghidra.

Do rozwiązania należy dołączyć skrypt **Makefile**, który w głównym katalogu wytworzy plik wykonywalny "**symbolize**" uruchamialny przez ``./symbolize path/to/in.elf path/to/out.elf``.
Dla ustalenia uwagi Makefile powinien działać na czystym Debianie 11 w dostarczonym obrazie do QEMU bądź Dockera (jeśli ktoś ma problemy wydajnościowe).
Sam Makefile może delegować budowanie do dowolnego innego systemu budowania bądź nawet nic nie robić.


Rozwiązanie prosimy przesłać w formie paczki na adres ``m.matraszek@mimuw.edu.pl`` z CC do ``a.jackowski@mimuw.edu.pl`` oraz ``w.ciszewski@mimuw.edu.pl``.
Spróbujemy umożliwić eksperymentalne oddawanie rozwiązań poprzez wydziałowy GitLab, który automatycznie weryfikowałby testy oraz ułatwi proces oceny kodu.



Wskazówki
---------
Dobrym źródłem inspiracji mogą być publikacje naukowe jak wskazana [DdisasmUSENIX]_.

Do deasemblacji polecamy bibliotekę Capstone_, która ma bindingi dostępne w wielu językach.
Do parsowania i edycji EFL-ów możemy polecić np. projekt LIEF_.
Warto zacząć od wersji minimum aby upewnić się, że umiemy stworzyć poprawny ELF
(np. wiele bibliotek umie tylko czytać albo nie umie stworzyć/zmienić ``STRTAB``).

Warto korzystać z poznanych na zajęciach narzędzi *binutils*.
Na przykład, niezwykle pomocne może być połączenie flag ``d`` i ``r`` w objdump: ``objdump -r -d a.elf``.
Gdybyśmy chcieli użyć clanga, odpowiednim targetem będzie: ``i586-intel-elfiamcu``.


.. literalinclude:: b-main.objdump
   :caption: objdump b.elf -r --disassemble=main
   :language: objdump
   :start-at: <main>:

.. _lektury całego postu:
.. _poście: https://gynvael.coldwind.pl/?lang=pl&id=777
.. _TIOBE: https://www.tiobe.com/tiobe-index/
.. _JetBrains: https://www.jetbrains.com/lp/devecosystem-2023/languages/
.. _Capstone: https://www.capstone-engine.org/
.. _pyeftools: https://github.com/eliben/pyelftools
.. _LIEF: https://lief-project.github.io/
.. [DdisasmUSENIX]  A. Flores-Montoya and E. Schulte, “Datalog Disassembly,” presented at the 29th USENIX Security Symposium (USENIX Security 20), 2020, pp. 1075–1092. Available: https://www.usenix.org/conference/usenixsecurity20/presentation/flores-montoya
.. _Datalog Disassembly: https://www.usenix.org/conference/usenixsecurity20/presentation/flores-montoya
.. _iamcu psABI: https://raw.githubusercontent.com/wiki/hjl-tools/x86-psABI/iamcu-psABI-0.7.pdf
.. _iamcu psABI repo: https://gitlab.com/x86-psABIs/i386-ABI/-/tree/hjl/mcu/empty

.. [#hp] Np. `luka w sterowniku drukarek HP odkryta po 16 latach <https://www.pcformat.pl/News-W-sterownikach-drukarek-HP-odkryto-nienaprawiona-od-16-lat-luke-,n,24483>`_
.. [#tsue] Polecam lekturę `wyroku TSUE z października 2021 r. w sprawie C-13/20 <https://curia.europa.eu/juris/liste.jsf?num=C-13/20&language=PL>`_.
.. [#tests] W szczególności można spodziewać się dodatkowych testów analogicznych do publicznych, sprawdzających, czy rozwiązanie nie overfittuje się do testów.
.. [#mcus] Na świecie jest o rząd wielkości więcej mikrokontrolerów niż procesorów, jakie znamy z komputera.
.. [#quark] Linia nie jest już aktywna, a repozytoria na GitHubie (QMSI_) zostały zarchiwizowane w 2022 r. Niemniej jednak można ją kolekcjonersko kupić_. (Podkreślić muszę, że to nic w tym zadaniu nie da).
.. [#startup] Domyślny plik startowy libc na mikrokontrolerze ładowałby sekcję ``.data`` z jej lokalizacji we Flashu (czyli z adresu fizycznego segmentu do wirtualnego).
.. [#gynvael] Zachęcam do `lektury całego postu`_ o rekompilacji i pociągach w tle.


.. Zephyr removed support in v2.0
.. _Zephyr board: https://github.com/zephyrproject-rtos/zephyr/tree/v1.14-branch/boards/x86/quark_se_c1000_devboard
.. _Zephyr SoC: https://github.com/zephyrproject-rtos/zephyr/tree/v1.14-branch/soc/x86/intel_quark

.. From QMSI linker scripts:
   Repos discontinued in 2022

.. _picolibc: https://github.com/picolibc/picolibc
.. _QMSI linker: https://github.com/quark-mcu/qmsi/tree/master/soc/quark_se/linker
.. _QMSI: https://github.com/quark-mcu/qmsi
.. _QMSI ld: https://github.com/quark-mcu/qm-bootloader/blob/master/2nd-stage/2nd_stage.ld
.. _Intel Quark: https://eu.mouser.com/applications/Intel-Quark-Internet-of-Things-MCU/
.. _kupić: https://kamami.pl/inne-zestawy-uruchomieniowe/574716-intel-quark-microcontroller-dev-kit-d2000-zestaw-deweloperski-z-mikrokontrolerem-intel-quark-d2000.html

.. Autor zadania - Maciek Matraszek
