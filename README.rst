================================
Publiczne repozytorium zadania 1
================================

.. important::
    Obowiązująca, wyrenderowana treść zadania jest dostępna pod https://students.mimuw.edu.pl/ZSO/PUBLIC-SO/2023-2024/z1_elf/.

Obecna wersja repozytorium zawiera poglądowe przedstawienie założeń i sposobu kompilacji programów testowych.
Publiczne testy i skrypt oceniający będą udostępnione do najbliższych zajęć.

Użycie
======

Skrypt ``Makefile`` zakłada zainstalowane ``x86_64-linux-gnu-gcc-10``.
Zawiera zestaw podstawowych flag opisanych w zadaniu oraz przykładowe flagi dodatkowe.

Wykonując ``make run-a run-b`` możemy skompilować i uruchomić przykładowe programy.

W katalogu ``picolibc`` znajduje się skompilowana wersja biblioteki standardowej, submoduł źródłowy oraz wymagany klej.

W katalogu ``examples`` znajdziemy dwa przykładowe programy testowe.
Program ``a`` jest samodzielny: nie jest linkowany z biblioteką standardową.
Program ``b`` pokazuje przykładową interakcję z picolibc.

W pliku ``examples/_io_impl.c`` znajduje się prosta implementacja wejścia/wyjścia dla naszego udawanego mikrokontrolera.

Pozostałe przydatne cele make (na przykładzie programu a):

``all``
    Kompiluje wszystkie programy testowe.
``examples/a.elf``
    Kompiluje tylko program ``a``.
``run-a``
    Uruchamia program ``a`` z przykładowym wejściem an stdin.
``examples/a.strip``
    Tworzy bazy plik, który będzie wejściem dla ``./symbolize``.
``examples/a.flash``
    Tworzy plik przypominający obrazów Flasha dla Quarka.
``examples/a-objdump``
    Pokazuje ładnie zdeasemblowany plik ``a.elf``.
    Co więcej, uruchamiając przez ``make examples/a-objdump  WITH_SYMBOLS=1`` pokaże relokacje, symbole i inne informacje.
``examples/a-run``
    Tak samo jak ``run-a``, ale nie jest zahardkodowanym skrótem.

Ponadto, uruchamiając ``make build-picolibc`` możemy przekompilować używaną bibliotekę standardową.
