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
