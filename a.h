#include<stdbool.h>

#define intAt(x, ix) ((int*) x->bytes)[ix]
#define kAt(x, ix) ((k*) x->bytes)[ix]

#define propogateError(x) if (x->error) return x

typedef struct k {
    signed char type;
    unsigned int refcount;
    bool error;
    union {
        int i; // atomic int
        char c; // atomic char
        struct{unsigned long n; unsigned char* bytes;}; // array (possibly nested)
    };
} *k;
