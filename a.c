#define _DEFAULT_SOURCE
#include"a.h"
#include"tokeniser.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>

static const char* BANNER = "arraylang (c) 2025 Caitanya Durley";
static char errorString[100];
static unsigned long workspace = 0;

struct variables {
    char** names;
    k* values;
    size_t count;
    size_t capacity;
};

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

static const char* MONADIC_VERB_NAMES[] = {"count", "neg", "enlist"};
static const k (*MONADIC_VERB_FUNCS[])(k) = {count, neg, enlist};
static const char* DYADIC_VERB_NAMES[] = {"take", "+", "-", "=", "join"};
static const k (*DYADIC_VERB_FUNCS[])(k, k) = {take, add, sub, equals, join};


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

long findString(const char* needle, const char** haystack, size_t n) {
    for (long i = 0; i < n; i++) {
        if (0 == strcmp(needle, haystack[i])) return i;
    }
    return -1;
}

bool isValidVarName(token name) {
    char* s = tokenToString(name);
    return s[0] >= 'a' && s[0] <= 'z' || s[0] >= 'A' && s[0] <= 'Z';
}

k evalNoun(char* s, struct variables variables) {
    char* strtolInvalidPtr;
    errno = 0;
    long parsedInt = strtol(s, &strtolInvalidPtr, 0);
    if (errno == 0 && *strtolInvalidPtr == '\0') return createInt(parsedInt);
    if (strtolInvalidPtr != s) {
        sprintf(errorString, "Couldn't parse as integer: %s", s);
        return createError(error_parse);
    }
    long varIx = findString(s, (const char**)variables.names, variables.count);
    if (varIx > -1) {
        k variable = variables.values[varIx];
        incRefcount(variable);
        return variable;
    }
    strncpy(errorString, s, sizeof(errorString) / sizeof(*errorString));
    return createError(error_undefined);
}

k eval(token* tokens, size_t tokenCount, struct variables variables) {
    if (tokenCount == 1) {
        // end of tape => currToken must be a noun
        return evalNoun(tokenToString(tokens[0]), variables);
    }
    long verb = findString(tokenToString(tokens[0]), MONADIC_VERB_NAMES, arrLength(MONADIC_VERB_NAMES));
    if (verb > -1) {
        k right = eval(tokens + 1, tokenCount - 1, variables);
        propogateError(right);
        return consume(right, MONADIC_VERB_FUNCS[verb](right));
    }
    // tokens[0] must be a noun, and tokens[1] should be a verb
    verb = findString(tokenToString(tokens[1]), DYADIC_VERB_NAMES, arrLength(DYADIC_VERB_NAMES));
    if (verb == -1) {
        sprintf(errorString, "Expected verb, got \"%s\"", tokenToString(tokens[1]));
        return createError(error_parse);
    }
    if (tokenCount < 3) {
        // dyadic verb with no right noun
        sprintf(errorString, "Projections unsupported");
        return createError(error_nyi);
    }
    k left = evalNoun(tokenToString(tokens[0]), variables);
    propogateError(left);
    k right = eval(tokens + 2, tokenCount - 2, variables);
    propogateError(right);
    return consume(left, consume(right, DYADIC_VERB_FUNCS[verb](left, right)));
}

k evalAssignment(token* tokens, size_t tokenCount, struct variables* variables) {
    token varName = tokens[0];
    if (!isValidVarName(varName)) {
        sprintf(errorString, "Invalid variable name: %s", tokenToString(varName));
        return createError(error_parse);
    }
    long varIx = findString(tokenToString(varName), (const char**) variables->names, variables->count);
    if (tokenCount < 3) {
        sprintf(errorString, "No value to assign to %s", tokenToString(varName));
        return createError(error_parse);
    }
    k val = eval(tokens + 2, tokenCount - 2, *variables);
    propogateError(val);
    incRefcount(val);
    if (varIx > -1) {
        decRefcount(variables->values[varIx]);
    } else {
        if (variables->count >= variables->capacity) {
            variables->capacity *= 2;
            variables->names = reallocarray(variables->names, variables->capacity, sizeof(*variables->names));
            variables->values = reallocarray(variables->values, variables->capacity, sizeof(*variables->values));
        }
        varIx = variables->count++;
        variables->names[varIx] = calloc(1 + strlen(tokenToString(varName)), sizeof(char));
        strcpy(variables->names[varIx], tokenToString(varName));
    }
    variables->values[varIx] = val;
    return val;
}

bool readline(FILE* stream, char** lineptr, size_t* n) {
    ssize_t num_chars = getline(lineptr, n, stream);
    if (num_chars <= 0) {
        return false;
    }
    // clamp the newline
    (*lineptr)[num_chars - 1] = 0;
    return true;
}

void debug(struct variables variables) {
    printf("Workspace: %lu\n", workspace);
    for (size_t i = 0; i < variables.count; i++) {
        printf("%s: type=%d, refcount=%u\n",
            variables.names[i],
            variables.values[i]->type,
            variables.values[i]->refcount
        );
    }
}

struct variables initVariables(size_t capacity) {
    struct variables out;
    out.names = calloc(capacity, sizeof(char*));
    out.values = calloc(capacity, sizeof(k));
    out.count = 0;
    out.capacity = capacity;
    return out;
}

int main(int argc, char* argv[]) {
    printf("%s\n", BANNER);
    bool batchMode = argc >= 2;
    FILE* stream = batchMode ? fopen(argv[1], "r") : stdin;
    if (stream == NULL) {
        fprintf(stderr, "Couldn't open %s for reading\n", batchMode ? argv[1] : "stdin");
        return 1;
    }
    char* line = NULL;
    size_t lineLength = 0;
    token* tokens = NULL;
    size_t tokensCapacity = 0;
    struct variables variables = initVariables(2);
    k result;
    while (batchMode?:write(1, "a) ", 3), readline(stream, &line, &lineLength)) {
        size_t tokenCount = tokeniser(line, &tokens, &tokensCapacity);
        if (!tokenCount) continue;
        if (tokenToString(tokens[0])[0] == '/') continue;
        if (0 == strcmp(tokenToString(tokens[0]), "debug")) {
            debug(variables);
            continue;
        }
        if (tokenCount >= 2 && 0 == strcmp(tokenToString(tokens[1]), ":")) {
            result = evalAssignment(tokens, tokenCount, &variables);
        } else {
            result = eval(tokens, tokenCount, variables);
        }
        print(result);
        decRefcount(result);
        printf("\n");
    }
    fclose(stream);
    return 0;
}
