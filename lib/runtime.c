#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void runtime_error(void)
{
    fprintf(stderr, "runtime error\n");
    exit(1);
}

void printInt(int x) { printf("%d\n", x); }

void printString(char *s)
{
    if (!s)
        s = "";
    printf("%s\n", s);
}

void error(void) { runtime_error(); }

int readInt(void)
{
    int x = 0;
    if (scanf("%d", &x) != 1)
        runtime_error();
    return x;
}

// Reads up to '\n' or EOF, returns malloc'ed string without trailing '\n'.
// IMPORTANT: skip leading '\n' / '\r' left by scanf.
char *readString(void)
{
    int c;

    // skip leading newlines (common after readInt)
    do
    {
        c = fgetc(stdin);
        if (c == EOF)
        {
            char *empty = (char *)malloc(1);
            if (!empty)
                runtime_error();
            empty[0] = '\0';
            return empty;
        }
    } while (c == '\n' || c == '\r');

    // put back first non-newline char
    ungetc(c, stdin);

    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
        runtime_error();

    while ((c = fgetc(stdin)) != EOF && c != '\n')
    {
        if (c == '\r')
            break; // handle CRLF
        if (len + 1 >= cap)
        {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb)
            {
                free(buf);
                runtime_error();
            }
            buf = nb;
        }
        buf[len++] = (char)c;
    }

    buf[len] = '\0';
    return buf;
}

char *__latte_concat(const char *a, const char *b)
{
    if (!a)
        a = "";
    if (!b)
        b = "";

    size_t la = strlen(a);
    size_t lb = strlen(b);

    char *r = (char *)malloc(la + lb + 1);
    if (!r)
        runtime_error();

    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

// --- allocations for arrays/objects (used by backend) ---

void *__latte_new_obj(int size)
{
    if (size < 0)
        runtime_error();

    // malloc(0) is allowed but may return NULL; we prefer at least 1 byte
    size_t n = (size == 0) ? 1u : (size_t)size;

    void *p = malloc(n);
    if (!p)
        runtime_error();

    // zero-init: fields default to 0 / null
    memset(p, 0, n);
    return p;
}

// Layout: [ int32 len | 4 bytes padding ] [ data... ]
// Returns pointer to the beginning of header (so length is at *(int*)arr).
void *__latte_new_array(int len, int elemSize)
{
    if (len < 0)
        runtime_error();
    if (elemSize != 4 && elemSize != 8)
        runtime_error(); // matches codegen choices

    const size_t header = 8; // keep aligned for pointers
    size_t dataBytes = (size_t)len * (size_t)elemSize;

    // overflow check (paranoia)
    if (len != 0 && dataBytes / (size_t)len != (size_t)elemSize)
        runtime_error();

    size_t total = header + dataBytes;
    if (total == 0)
        total = 1;

    unsigned char *p = (unsigned char *)malloc(total);
    if (!p)
        runtime_error();

    memset(p, 0, total);

    // store length
    *(int *)p = len;

    return (void *)p;
}
