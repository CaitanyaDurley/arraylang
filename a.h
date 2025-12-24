typedef enum {
    type_mixed,
    type_error,
    type_int,
    type_char,
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
        struct{unsigned long n; unsigned char* bytes;}; // array (possibly nested)
    };
} *k;

#define intAt(x, ix) ((int*) x->bytes)[ix]
#define kAt(x, ix) ((k*) x->bytes)[ix]

#define propogateError(x) if (x->type == -type_error) return x
