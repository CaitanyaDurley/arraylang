#include<stdbool.h>

typedef enum {
    type_mixed,
    type_error,
    type_int,
    type_char,
    type_bool,
} type;

typedef enum {
    error_type,
    error_length,
    error_parse,
    error_nyi,
    error_undefined,
} error;

typedef struct k {
    signed char type;
    unsigned int refcount;
    union {
        error e; // type of error
        int i; // atomic int
        char c; // atomic char
        bool b; // atomic bool
        struct{unsigned long n; unsigned char* bytes;}; // array (possibly nested)
    };
} *k;

#define kAt(x, ix) ((k*) x->bytes)[ix]
#define intAt(x, ix) ((int*) x->bytes)[ix]
#define charAt(x, ix) ((char*) x->bytes)[ix]
#define boolAt(x, ix) ((bool*) x->bytes)[ix]

#define propogateError(x) if (x->type == -type_error) return x

#define eachNestedK(x, f) if (x->type == type_mixed) for (unsigned long i = 0; i < length(x); i++) f(kAt(x, i))

#define provide(x) ({incRefcount(x); x;})
#define consume(x, e) ({k out = e; decRefcount(x); out;})

#define swap(x, y) k tmp = x; x = y; y = tmp

#define unreachable puts("Sorry, a fatal error has been encountered and this process will now exit"); exit(1)

#define arrLength(x) sizeof(x) / sizeof(*x)
