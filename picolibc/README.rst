============================
Pomocnicze elementy picolibc
============================

``_repo``
    Jest submodułem wskazującym na ustaloną na to zadanie wersję picolibc.
``iamcu``
    Zawiera zbudowaną wersję picolibc z odpowiednią dla nas konfiguracją.
    Ten katalog można zaktualizować uruchamiając w głównym katalogu repozytorium ``make build-picolibc``.
    Możliwe, że pojawią się wersję z innymi flagami kompilatora.
``crt0.c``
    Zawiera wersję ``_repo/picocrt/shared/crt0.c``, która jest odpowiednia dla naszego "udawania", czyli dla aplikacji będącej uruchamianej z systemem operacyjnym.
    To znaczy: a) nie ładuje segmentu danych, bo zrobiło to już jądro, b) nie zakłada bycia uruchomionym w trybie 16-bitowym.
``picolibc_syscalls.c``
    Zawiera minimalną implementację stdio wymaganą przez picolibc.
``picolibc-quarkish-linux-none.txt``
    Zawiera flagi kompilatora użyte przy budowaniu biblioteki.
