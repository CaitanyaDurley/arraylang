#include"a.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

static const char* BANNER = "arraylang (c) 2025 Caitanya Durley";
static const size_t MAX_LINE_LENGTH = 99;
static char* line;

// Memory management

size_t typeSize(signed char type) {
    switch (abs(type)) {
        case 0:
            return sizeof(k*);
        case 1:
            return sizeof(int);
        case 2:
            return sizeof(char);
    }
}

k allocate(unsigned long length, unsigned char type) {
    k out;
    out.type = type;
    out.error = false;
    out.n = length;
    out.bytes = malloc(length * typeSize(type));
    return out;
}

unsigned long length(k x) {
    return x.type < 0 ? 1 : x.n;
}

k createString(char* s) {
    k out;
    out.error = false;
    out.n = strlen(s);
    out.bytes = s;
    return out;
}

void copy(k from, k to, unsigned long offset) {
    size_t num_bytes = from.n * typeSize(from.type);
    memcpy(to.bytes + offset * typeSize(to.type), from.bytes, num_bytes);
}

// Type wrangling

bool isAtom(k x) {
    return x.type < 0;
}

k createInt(int x) {
    k out;
    out.error = false;
    out.type = -1;
    out.i = x;
    return out;
}

k createError(char* message) {
    k out = createString(message);
    out.error = true;
    return out;
}

// Verb definitions

k count(k x) {
    return createInt(length(x));
}

k enlist(k x) {
    unsigned int t = x.type < 0 ? -x.type : 0;
    k out = allocate(1, t);
    switch (out.type) {
        case 0:
            // TODO: out is pointing to the copied struct (x) not the original
            // TODO: ref counting
            kAt(out, 0) = &x;
            break;
        case 1:
            intAt(out, 0) = x.i;
            break;
        case 2:
            out.bytes[0] = x.c;
            break;
    }
    return out;
}

k join(k x, k y) {
    if (abs(x.type) != abs(y.type)) {
        return createError("Can't join mismatched types");
    }
    x = isAtom(x) ? enlist(x) : x;
    y = isAtom(y) ? enlist(y) : y;
    k out = allocate(length(x) + length(y), x.type);
    copy(x, out, 0);
    copy(y, out, length(x));
    return out;
}

k neg(k x) {
    if (abs(x.type) != 1) {
        return createError("Can't negate non-numeric type");
    }
    if (isAtom(x)) {
        x.i = -x.i;
        return x;
    };
    // TODO: reuse x if refcount allows
    k out = allocate(length(x), x.type);
    for (int i = 0; i < length(x); i++) {
        intAt(out, i) = -intAt(x, i);
    }
    return out;
}

static const char* VERBS = " #-,";
static const k (*monadics[])(k) = {0, count, neg, enlist};
static const k (*dyadics[])(k, k) = {0, 0, 0, join};

// Output formatting

void print(k x) {
    if (x.error) {
        printf("ERROR: %s", (char*) x.bytes);
        return;
    }
    if (x.type == -1) {
        printf("%d", x.i);
        return;
    }
    if (x.type == -2) {
        printf("\"%c\"", x.c);
        return;
    }
    if (x.type == 2) {
        printf("\"%s\"", (char*) x.bytes);
        return;
    }
    printf("[");
    unsigned long n = length(x);
    for (int i = 0; i < n; i++) {
        switch (x.type) {
            case 0:
                print(*kAt(x, i));
                break;
            case 1:
                printf("%d", intAt(x, i));
                break;
        }
        printf(i == n - 1 ? "]" : ", ");
    }
}

// Input parsing

unsigned char parseVerb(char v) {
    // return ix of v in VERBS or 0 if dne
    return (strchr(VERBS, v) ?: VERBS) - VERBS;
}

k evalNoun(char c) {
    if (c < '0' || c > '9') {
        return createError("Unrecognised noun");
    }
    return createInt(c - '0');
}

k eval(char *s) {
    char *t = s;
    char i = s[0];
    t++;
    if (!*t) {
        // end of tape => i must be a noun
        return evalNoun(i);
    }
    unsigned char verb = parseVerb(i);
    if (verb) {
        k right = eval(t);
        propogateError(right);
        return monadics[verb](right);
    }
    // i must be a noun, and t should be a verb
    k right = eval(t + 1);
    propogateError(right);
    k left = evalNoun(i);
    propogateError(left);
    verb = parseVerb(*t);
    if (!verb) return createError("Expected verb");
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
