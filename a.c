#include"a.h"
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<unistd.h>

static const char* BANNER = "arraylang (c) 2025 Caitanya Durley";
static const size_t MAX_LINE_LENGTH = 99;
static char* line;

// Memory management

u64 allocate(unsigned char length) {
    unsigned char *v = malloc(length + 2);
    *v = 0; // ref count
    v++;
    *v = length; // length
    v++;
    return (u64) v;
}

void copy(u64 from, u64 to, unsigned char num_bytes) {
    memcpy((unsigned char*) to, (unsigned char*) from, num_bytes);
}

// Type wrangling

bool isAtom(u64 x) {
    return x < 256;
}

int atomToInt(u64 x) {
    return (int) x < 128 ? x : x - 256;
}

// Verb definitions

u64 count(u64 x) {
    if (isAtom(x)) {
        return 1;
    }
    return at(x, -1);
}

u64 enlist(u64 x) {
    // TODO: nested lists currently broken since pointer
    // is bigger than 1 byte
    u64 out = allocate(1);
    at(out, 0) = x;
    return out;
}

u64 join(u64 x, u64 y) {
    x = isAtom(x) ? enlist(x) : x;
    y = isAtom(y) ? enlist(y) : y;
    u64 out = allocate(count(x) + count(y));
    copy(x, out, count(x));
    copy(y, out + count(x), count(y));
    return out;
}

u64 neg(u64 x) {
    if (isAtom(x)) {
        // get the underflowed long back to < 256 so it
        // is recognised as an atom
        return (unsigned char) -x;
    };
    u64 out = allocate(count(x));
    for (int i = 0; i < count(x); i++) {
        at(out, i) = -at(x, i);
    }
    return out;
}

static const char* VERBS = " #-,";
static const u64 (*monadics[])(u64) = {0, count, neg, enlist};
static const u64 (*dyadics[])(u64, u64) = {0, 0, 0, join};

// Output formatting

void print(u64 x) {
    if (isAtom(x)) {
        printf("%d", atomToInt(x));
        return;
    }
    if (x == ERROR) {
        printf("ERROR");
        return;
    }
    printf("[");
    for (int i = 0; i < count(x) - 1; i++) {
        print(at(x, i));
        printf(", ");
    }
    print(at(x, count(x) - 1));
    printf("]");
}

// Input parsing

u64 parseVerb(char v) {
    // return ix of v in VERBS or 0 if dne
    return (strchr(VERBS, v) ?: VERBS) - VERBS;
}

u64 evalNoun(char c) {
    if (c < '0' || c > '9') {
        return ERROR;
    }
    return c - '0';
}

u64 eval(char *s) {
    char *t = s;
    char i = s[0];
    t++;
    if (!*t) {
        // end of tape => i must be a noun
        return evalNoun(i);
    }
    u64 verb = parseVerb(i);
    if (verb) {
        u64 right = eval(t);
        propogateError(right);
        return monadics[verb](right);
    }
    // i must be a noun, and t should be a verb
    u64 right = eval(t + 1);
    propogateError(right);
    u64 left = evalNoun(i);
    propogateError(left);
    verb = parseVerb(*t);
    panic(!verb);
    return dyadics[verb](left, right);
}

bool readline(void) {
    printf("\na) ");
    fflush(stdout);
    ssize_t num_bytes = read(0, line, MAX_LINE_LENGTH);
    if (-1 == num_bytes) {
        return false;
    }
    line[num_bytes - 1] = 0;
    return true;
}

int main(int argc, char* argv[]) {
    printf("%s\n", BANNER);
    line = malloc(MAX_LINE_LENGTH + 1);
    while (readline()) {
        if (!*line) continue;
        if (*line == '/') continue;
        print(eval(line));
    }
    free(line);
    return 0;
}
