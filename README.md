# Latte compiler

## Wymagane narzędzia (uruchamiane na WSL):
- `g++` (C++17) + `gcc`  
- `make`  
- BNFC (tylko przy ponownym generowaniu plików z `.cf`)  
- Bison >= 1.875  
- Flex  >= 2.5.4

## SetUp:
(jeżeli nie ma bnfc)  
bnfc -m --cpp -o .\src\frontend .\src\LatteCPP.cf  
make  
Gotowe pliki  
- latc.exe  
- lat_x86_64.exe

## Źródła:
https://bnfc.digitalgrammars.com/tutorial/bnfc-tutorial.html  
https://www.mimuw.edu.pl/~ben/Zajecia/Mrj2025/Latte/  
https://bnfc.readthedocs.io/en/latest/lbnf.html











# Latte Compiler – Frontend

Implementacja frontendu kompilatora języka Latte (parsowanie i analiza semantyczna).  
Projekt napisany w C++17 zgodnie z wymaganiami laboratorium MRJP.

## Budowanie

W katalogu głównym:

    make

Powstaje plik wykonywalny:

    ./latc

## Uruchamianie

    ./latc <plik.lat>

Zachowanie:

-   przy poprawnym programie: pierwsza linia stderr = "OK", kod wyjścia 0
-   przy błędzie: pierwsza linia stderr = "ERROR", kolejne linie zawierają opis błędu, kod wyjścia różny od zera

## Struktura projektu

Plik wykonywalny `latc` znajduje się w katalogu głównym.

## Narzędzia

-   C++17
-   brak dodatkowych bibliotek

## Rozszerzenia

Brak. Projekt obejmuje jedynie frontend.

## Użyte materiały i inspiracje

-   Wsparcie narzędziowe, struktura projektu (szkielety funkcji) oraz część dokumentacji przygotowane z pomocą ChatGPT (organizacja kodu, Makefile, README).


frontend // bnfc fieles = leksycal analysis, parser -> ast tree
semantic // semantic analysis files
latte_error // pretty error
Env // symbol table and scope management through map stack
