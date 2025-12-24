#include"a.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<stdbool.h>

static const char* BANNER = "arraylang (c) 2025 Caitanya Durley";
static char errorString[100];
static const size_t MAX_LINE_LENGTH = 99;
static char* line;

// Memory management and type wrangling

bool isAtom(k x) {
    return x->type < 0;
}

size_t typeSize(signed char type) {
    switch (abs(type)) {
        case type_mixed:
            return sizeof(k);
        case type_error:
            return sizeof(error);
        case type_int:
            return sizeof(int);
        case type_char:
            return sizeof(char);
    }
}

k create(signed char type) {
    k out = malloc(sizeof(struct k));
    out->type = type;
    out->refcount = 1;
    return out;
}

k createInt(int x) {
    k out = create(-type_int);
    out->i = x;
    return out;
}

k createVector(unsigned long length, unsigned char type) {
    k out = create(type);
    out->n = length;
    out->bytes = malloc(length * typeSize(type));
    return out;
}

k createString(char* s) {
    k out = create(type_char);
    out->n = strlen(s);
    out->bytes = s;
    return out;
}

k createError(error e) {
    k out = create(-type_error);
    out->e = e;
    return out;
}

unsigned long length(k x) {
    return x->type < 0 ? 1 : x->n;
}

void copy(k from, k to, unsigned long offset) {
    size_t num_bytes = from->n * typeSize(from->type);
    memcpy(to->bytes + offset * typeSize(to->type), from->bytes, num_bytes);
}

void incRefcount(k x) {
    x->refcount++;
}

void drop(k x); // forward declaration for decRefcount
void decRefcount(k x) {
    x->refcount--;
    if (0 == x->refcount) {
        drop(x);
    }
}

void drop(k x) {
    if (x->type == type_mixed) {
        for (unsigned long i = 0; i < length(x); i++) {
            decRefcount(kAt(x, i));
        }
    }
    if (! isAtom(x)) {
        free(x->bytes);
    }
    free(x);
}

// Verb definitions

k count(k x) {
    return createInt(length(x));
}

k enlist(k x) {
    unsigned int t = x->type < 0 ? -x->type : 0;
    k out = createVector(1, t);
    switch (out->type) {
        case type_mixed:
            incRefcount(x);
            kAt(out, 0) = x;
            break;
        case type_int:
            intAt(out, 0) = x->i;
            break;
        case type_char:
            out->bytes[0] = x->c;
            break;
    }
    return out;
}

k join(k x, k y) {
    if (abs(x->type) != abs(y->type)) {
        sprintf(errorString, "Can't join type %d to type %d", x->type, y->type);
        return createError(error_type);
    }
    x = isAtom(x) ? enlist(x) : x;
    y = isAtom(y) ? enlist(y) : y;
    k out = createVector(length(x) + length(y), x->type);
    copy(x, out, 0);
    copy(y, out, length(x));
    return out;
}

k neg(k x) {
    if (abs(x->type) != type_int) {
        sprintf(errorString, "Can't negate type %d", x->type);
        return createError(error_type);
    }
    if (isAtom(x)) {
        return createInt(-x->i);
    };
    // TODO: reuse x if refcount allows
    k out = createVector(length(x), x->type);
    for (int i = 0; i < length(x); i++) {
        intAt(out, i) = -intAt(x, i);
    }
    return out;
}

static const char* VERBS = " #-,";
static const k (*monadics[])(k) = {0, count, neg, enlist};
static const k (*dyadics[])(k, k) = {0, 0, 0, join};

// Output formatting

void printError(error e) {
    switch (e) {
        case error_type:
            printf("Type error: ");
            break;
        case error_length:
            printf("Length error: ");
            break;
        case error_parse:
            printf("Parse error: ");
            break;
        case error_nyi:
            printf("Not yet implemented error: ");
            break;
    }
    printf("%s", errorString);
}

void print(k x) {
    if (x->type == -type_error) {
        printError(x->e);
        return;
    }
    if (x->type == -type_int) {
        printf("%d", x->i);
        return;
    }
    if (x->type == -type_char) {
        printf("\"%c\"", x->c);
        return;
    }
    if (x->type == type_char) {
        printf("\"%s\"", (char*) x->bytes);
        return;
    }
    printf("[");
    unsigned long n = length(x);
    for (int i = 0; i < n; i++) {
        switch (x->type) {
            case type_mixed:
                print(kAt(x, i));
                break;
            case type_int:
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
        sprintf(errorString, "Expected noun, got %c", c);
        return createError(error_parse);
    }
    return createInt(c - '0');
}

k eval(char *s) {
    char* t = s + 1;
    char currToken = s[0];
    if (!*t) {
        // end of tape => currToken must be a noun
        return evalNoun(currToken);
    }
    unsigned char verb = parseVerb(currToken);
    if (verb) {
        k right = eval(t);
        propogateError(right);
        k out = monadics[verb](right);
        decRefcount(right);
        return out;
    }
    // currToken must be a noun, and t should be a verb
    k left = evalNoun(currToken);
    propogateError(left);
    verb = parseVerb(*t);
    if (!verb) {
        sprintf(errorString, "Expected verb, got %c", *t);
        return createError(error_parse);
    }
    t++;
    if (!*t) {
        // dyadic verb with no right noun
        sprintf(errorString, "Projections unsupported");
        return createError(error_nyi);
    }
    k right = eval(t);
    propogateError(right);
    k out = dyadics[verb](left, right);
    decRefcount(left);
    decRefcount(right);
    return out;
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
        k result = eval(line);
        print(result);
        decRefcount(result);
    }
    free(line);
    return 0;
}
