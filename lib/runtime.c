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

// Reads up to '\n' or EOF, returns malloc'ed string without trailing '\n'.
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

    // Handle CRLF
    if (len > 0 && buf[len - 1] == '\r') len--;

    buf[len] = '\0';
    return buf;
}

// Latte string concatenation helper: returns malloc'ed result.
char* __latte_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";

    size_t la = strlen(a);
    size_t lb = strlen(b);

    char* r = (char*)malloc(la + lb + 1);
    if (!r) runtime_error();

    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1); // copy '\0' too
    return r;
}
