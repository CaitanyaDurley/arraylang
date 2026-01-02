#define _DEFAULT_SOURCE
#include"tokeniser.h"
#include<stdio.h>
#include<stdbool.h>
#include<stdlib.h>
#include<string.h>

char* tokenToString(token t) {
    return t.token;
}

bool isValidTokenContinuation(char c) {
    return c >= 'a' && c <= 'z' ||
        c >= 'A' && c <= 'Z' ||
        c >= '0' && c <= '9' ||
        c == '_';
}

token allocateToken(size_t capacity) {
    token out;
    out.capacity = capacity;
    out.token = calloc(capacity, sizeof(char));
    return out;
}

void resizeToken(token* token, size_t capacity) {
    token->token = reallocarray(token->token, capacity, sizeof(char));
    token->capacity = capacity;
}

size_t tokeniser(char* s, token** tokensptr, size_t* n) {
    if (*tokensptr == NULL && *n == 0) {
        *n = 1;
        *tokensptr = calloc(1, sizeof(token));
    }
    size_t tokenIx = 0;
    size_t tokenLength;
    while (*s) {
        if (*s == ' ') {
            s++;
            continue;
        }
        if (tokenIx >= *n) {
            *n *= 2;
            *tokensptr = reallocarray(*tokensptr, *n, sizeof(token));
        }
        token* tokens = *tokensptr;
        if (!tokens[tokenIx].capacity) {
            tokens[tokenIx] = allocateToken(2);
        };
        tokenLength = 1;
        if (isValidTokenContinuation(*s++)) {
            // how long is this token?
            while (isValidTokenContinuation(*s)) {
                tokenLength++;
                s++;
            };
        }
        token t = tokens[tokenIx];
        if (t.capacity < tokenLength + 1) resizeToken(&t, tokenLength + 1);
        memcpy(t.token, s - tokenLength, tokenLength);
        t.token[tokenLength] = 0;
        tokenIx++;
    }
    return tokenIx;
}
