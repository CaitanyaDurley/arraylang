#include"a.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

static const char* BANNER = "arraylang (c) 2025 Caitanya Durley";
static char errorString[100];
static char* line = NULL;
static size_t lineLength = 0;
static k variables[26];
static unsigned long workspace = 0;

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
        case type_bool:
            return sizeof(bool);
        default:
            unreachable;
    }
}

k create(signed char type) {
    k out = malloc(sizeof(struct k));
    workspace += sizeof(struct k);
    out->type = type;
    out->refcount = 1;
    return out;
}

k createInt(int x) {
    k out = create(-type_int);
    out->i = x;
    return out;
}

k createChar(char x) {
    k out = create(-type_char);
    out->c = x;
    return out;
}

k createBool(bool x) {
    k out = create(-type_bool);
    out->b = x;
    return out;
}

k createVector(unsigned long length, unsigned char type) {
    k out = create(type);
    out->n = length;
    out->bytes = malloc(length * typeSize(type));
    workspace += length * typeSize(type);
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
    eachNestedK(x, decRefcount);
    if (! isAtom(x)) {
        free(x->bytes);
        workspace -= x->n * typeSize(x->type);
    }
    free(x);
    workspace -= sizeof(struct k);
}

// K utils

bool areBroadcastable(k x, k y) {
    if (isAtom(x) || isAtom(y) || length(x) == length(y)) {
        return true;
    }
    sprintf(errorString, "Incompatible lengths: %lu, %lu", length(x), length(y));
    return false;
}

bool arePromotable(k x, k y) {
    // TODO: other type promotion
    if (abs(x->type) == abs(y->type) || x->type == type_mixed || y->type == type_mixed) {
        return true;
    }
    sprintf(errorString, "Incompatible types: %d, %d", x->type, y->type);
    return false;
}

int max(int a, int b) {
    return a < b ? b : a;
}

// Callers must ensure x is an array
k at(k x, unsigned long ix) {
    switch (x->type) {
        case type_mixed:
            return provide(kAt(x, ix));
        case type_int:
            return createInt(intAt(x, ix));
        case type_char:
            return createChar(charAt(x, ix));
        case type_bool:
            return createBool(boolAt(x, ix));
        default:
            unreachable;
    }
}

// Callers must ensure abs(x->type) == abs(y->type) != 0
k cmp(k x, k y) {
    if (isAtom(x) && isAtom(y)) {
        switch (-x->type) {
            case type_int:
                return createBool(x->i == y->i);
            case type_char:
                return createBool(x->c == y->c);
            case type_bool:
                return createBool(x->b == y->b);
            default:
                unreachable;
        }
    }
    k out = createVector(max(length(x), length(y)), type_bool);
    for (unsigned long i = 0; i < out->n; i++) {
        switch (abs(x->type)) {
            case type_int:
                boolAt(out, i) = (isAtom(x) ? x->i : intAt(x, i)) == (isAtom(y) ? y->i : intAt(y, i));
                break;
            case type_char:
                boolAt(out, i) = (isAtom(x) ? x->c : charAt(x, i)) == (isAtom(y) ? y->c : charAt(y, i));
                break;
            case type_bool:
                boolAt(out, i) = (isAtom(x) ? x->b : boolAt(x, i)) == (isAtom(y) ? y->b : boolAt(y, i));
                break;
            default:
                unreachable;
        }
    }
    return out;
}

// Verb definitions

k nyi() {
    errorString[0] = '\0';
    return createError(error_nyi);
}

k count(k x) {
    return createInt(length(x));
}

k take(k x, k y) {
    if (x->type != -type_int) {
        sprintf(errorString, "Non-integer number of items to take");
        return createError(error_type);
    }
    if (x->i < 0) return nyi();
    unsigned long n = length(y);
    k out = createVector(x->i, abs(y->type));
    for (unsigned long i = 0; i < length(out); i++) {
        intAt(out, i) = isAtom(y) ? y->i : intAt(y, i % n);
    }
    return out;
}

k enlist(k x) {
    unsigned int t = x->type < 0 ? -x->type : 0;
    k out = createVector(1, t);
    switch (t) {
        case type_mixed:
            incRefcount(x);
            kAt(out, 0) = x;
            break;
        case type_int:
            intAt(out, 0) = x->i;
            break;
        case type_char:
            charAt(out, 0) = x->c;
            break;
        case type_bool:
            boolAt(out, 0) = x->b;
            break;
        default:
            unreachable;
    }
    return out;
}

k join(k x, k y) {
    if (!arePromotable(x, y)) return createError(error_type);
    if (isAtom(x)) {
        x = enlist(x);
        return consume(x, join(x, y));
    }
    if (isAtom(y)) {
        y = enlist(y);
        return consume(y, join(x, y));
    }
    k out = createVector(x->n + y->n, x->type);
    copy(x, out, 0);
    copy(y, out, x->n);
    eachNestedK(out, incRefcount);
    return out;
}

k add(k x, k y) {
    if (abs(x->type) != type_int || abs(y->type) != type_int) {
        sprintf(errorString, "Can't add type %d to type %d", x->type, y->type);
        return createError(error_type);
    }
    if (!(areBroadcastable(x, y))) {
        return createError(error_length);
    }
    if (isAtom(x) && isAtom(y)) {
        return createInt(x->i + y->i);
    }
    if (isAtom(x)) {
        swap(x, y);
    }
    k out = createVector(length(x), type_int);
    for (unsigned long i = 0; i < length(out); i++) {
        intAt(out, i) = intAt(x, i) + (isAtom(y) ? y->i : intAt(y, i));
    }
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

k sub(k x, k y) {
    y = neg(y);
    propogateError(y);
    return consume(y, add(x, y));
}

k equals(k x, k y) {
    if (!arePromotable(x, y)) return createError(error_type);
    if (!areBroadcastable(x, y)) return createError(error_length);
    if (y->type == type_mixed) {
        swap(x, y);
    }
    if (x->type == type_mixed) {
        k out = createVector(x->n, type_mixed);
        k yi;
        for (unsigned long i = 0; i < x->n; i++) {
            yi = isAtom(y) ? provide(y) : at(y, i);
            kAt(out, i) = consume(yi, equals(kAt(x, i), yi));
        }
        return out;
    }
    return cmp(x, y);
}

static const char* VERBS = " #-+,=";
static const k (*monadics[])(k) = {0, count, neg, nyi, enlist, nyi};
static const k (*dyadics[])(k, k) = {0, take, sub, add, join, equals};

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
        case error_undefined:
            printf("Undefined error: ");
            break;
        default:
            unreachable;
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
    if (x->type == -type_bool) {
        printf("%s", x->b ? "true" : "false");
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
            case type_bool:
                printf("%s", boolAt(x, i) ? "true" : "false");
                break;
            default:
                unreachable;
        }
        printf(i == n - 1 ? "]" : ", ");
    }
}

// Input parsing

unsigned char parseVerb(char v) {
    // return ix of v in VERBS or 0 if dne
    return (strchr(VERBS, v) ?: VERBS) - VERBS;
}

bool isVarName(char c) {
    return c >= 'a' && c <= 'z';
}

k evalNoun(char c) {
    if (isVarName(c)) {
        k variable = variables[c - 'a'];
        if (variable) {
            incRefcount(variable);
            return variable;
        } else {
            sprintf(errorString, "%c", c);
            return createError(error_undefined);
        }
    }
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
        return consume(right, monadics[verb](right));
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
    return consume(left, consume(right, dyadics[verb](left, right)));
}

k evalAssignment(char* s) {
    char varName = s[0];
    if (!isVarName(varName)) {
        sprintf(errorString, "Invalid variable name: %c", varName);
        return createError(error_parse);
    }
    unsigned int varIx = varName - 'a';
    if (!*(s + 2)) {
        sprintf(errorString, "No value to assign");
        return createError(error_parse);
    }
    k val = eval(s + 2);
    propogateError(val);
    incRefcount(val);
    if (variables[varIx]) {
        decRefcount(variables[varIx]);
    }
    variables[varIx] = val;
    return val;
}

bool readline(FILE* stream) {
    ssize_t num_chars = getline(&line, &lineLength, stream);
    if (num_chars <= 0) {
        return false;
    }
    // clamp the newline
    line[num_chars - 1] = 0;
    return true;
}

void debug(void) {
    printf("Workspace: %lu\n", workspace);
    for (int i = 0; i < 26; i++) {
        if (variables[i]) {
            printf("%c: type=%d, refcount=%u\n", 'a'+i, variables[i]->type, variables[i]->refcount);
        }
    }
}

int main(int argc, char* argv[]) {
    printf("%s\n", BANNER);
    bool batchMode = argc >= 2;
    FILE* stream = batchMode ? fopen(argv[1], "r") : stdin;
    if (stream == NULL) {
        fprintf(stderr, "Couldn't open %s for reading\n", batchMode ? argv[1] : "stdin");
        return 1;
    }
    while (batchMode?:write(1, "a) ", 3), readline(stream)) {
        if (!*line) continue;
        if (*line == '/') continue;
        if (0 == strcmp(line, "debug")) {
            debug();
            continue;
        }
        k result;
        if (line[1] == ':') {
            result = evalAssignment(line);
        } else {
            result = eval(line);
        }
        print(result);
        decRefcount(result);
        printf("\n");
    }
    fclose(stream);
    return 0;
}
