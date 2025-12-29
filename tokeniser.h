#ifndef TOKENISER_H_INCLUDED
#define TOKENISER_H_INCLUDED
#include<stddef.h>

typedef struct {
    char* token;
    size_t capacity;
} token;

char* tokenToString(token t);
size_t tokeniser(char* s, token** tokensptr, size_t* n);

#endif
