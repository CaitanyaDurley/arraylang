// 0...127 => 0...127
// 128...255 => -128...-1
// 256 => ERROR
// 257+ => pointer to vector
typedef unsigned long u64;
static const u64 ERROR = 256;

#define at(vec, ix) ((unsigned char*) vec)[ix]

#define propogateError(val) if (val == ERROR) return ERROR
#define panic(expr) if (expr) return ERROR
