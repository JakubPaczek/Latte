// lib/runtime.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void runtime_error(void) {
    fprintf(stderr, "runtime error\n");
    exit(1);
}

void printInt(int x) {
    printf("%d\n", x);
}

void printString(char* s) {
    if (!s) s = "";
    printf("%s\n", s);
}

void error(void) {
    runtime_error();
}

int readInt(void) {
    int x = 0;
    if (scanf("%d", &x) != 1) {
        runtime_error();
    }
    return x;
}

// Czyta do '\n' lub EOF, zwraca malloc-owany string bez trailing '\n'.
// Działa cross-platform i nie potrzebuje getline/ssize_t.
char* readString(void) {
    size_t cap = 64;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) runtime_error();

    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); runtime_error(); }
            buf = nb;
        }
        buf[len++] = (char)c;
    }

    // usuń ewentualny '\r' (CRLF)
    if (len > 0 && buf[len - 1] == '\r') len--;

    buf[len] = '\0';
    return buf;
}

int __divsi3(int a, int b) {
    if (b == 0) {
        error();
    }
    return a / b;
}

int __modsi3(int a, int b) {
    if (b == 0) {
        error();
    }
    return a % b;
}

