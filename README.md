# Latte compiler

## Wymagane narzędzia (uruchamiane na WSL):

- `g++` (C++17) + `gcc`
- `make`
- BNFC (tylko przy ponownym generowaniu plików z `.cf`)
- Bison >= 1.875
- Flex >= 2.5.4

## SetUp:

(jeżeli nie ma bnfc)  
bnfc -m --cpp -o .\src\frontend .\src\LatteCPP.cf  
make  
Gotowe pliki

- latc (frontend)
- lat_x86_64 (pełny kompilator)

## Źródła:

https://bnfc.digitalgrammars.com/tutorial/bnfc-tutorial.html  
https://www.mimuw.edu.pl/~ben/Zajecia/Mrj2025/Latte/  
https://bnfc.readthedocs.io/en/latest/lbnf.html
Writing a C Compiler Nora Sandler - fragmenty kodu
ChatGPT - szkielet rozwiązania (ustalenie kolejności, wzór klas, struktur, typów), poprawa błędów związanych z rozszerzeniem gramatyki LatteCPP.cf, runtime.c, skrypty bash do uruchamiania testów, Makefile poprawa zależności, komentarze
