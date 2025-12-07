#include "variable.h"
#include "def.h"
#include <stdio.h>

// Create a new variable with a type
Variable var_new(VarType type) {
  Variable v = (Variable){0};
  v.type = type;
  switch(type) {
  case VarInteger:
    v.int_val = 0;
    break;
  case VarReal:
    v.real_val = 0.f;
    break;
  case VarBoolean:
    v.boolean_val = 0;
    break;
  case VarCharacter:
    v.character_val = 0;
    break;
  default:
    PERROR("Invalid variable type %d\n", type);
    v.type = -1;
    return v;
  }

  return v;
}

// Destroy a variable
// Unneeded for now, but i will need to free memory when i add arrays.
static void variable_destroy(Variable *v) {
  v->type = -1;
  return;
}

// Copy a variable by value
// Also unneeded until arrays
Variable variable_copy(Variable b) {
  return b;
}

// Assign variable a = variable b
int var_assign(Variable *a, Variable *b) {
  if(a->type == -1) {
    PERROR("Trying to assign to a null variable\n");
    return 1;
  }

  if(b->type == -1) {
    PERROR("Trying to assign with a null value\n");
    return 1;
  }

  variable_destroy(a);
  *a = variable_copy(*b);
}
