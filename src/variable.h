#ifndef VARIABLE_H
#define VARIABLE_H

// Includes
#include "parser.h"

// Structs
typedef struct {
  VarType type;
  union {
    int int_val;
    float real_val;
    int boolean_val;
    char character_val;
  };
} Variable;

// Function prototypes
Variable var_new(VarType type);
int var_assign(Variable *a, Variable *b);

#endif // variable.h
