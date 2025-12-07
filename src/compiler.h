#ifndef COMPILER_H
#define COMPILER_H

// Includes
#include "parser.h"
#include <stdio.h>

// Structs
typedef struct {
  FILE *out_file;
  size_t indent;
} Compiler;

// Function prototypes
int compile(Parser *parser, FILE *out_file);

#endif // compiler.h
