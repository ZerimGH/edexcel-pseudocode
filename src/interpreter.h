#ifndef INTERPRETER_H
#define INTERPRETER_H

// Includes
#include "parser.h"
#include "variable.h"

typedef struct FrameNode {
  Variable var;
  struct FrameNode *next;
  char *id;
} FrameNode;

#define NUM_BUCKETS 1024

typedef struct {
  FrameNode *buckets[NUM_BUCKETS];
} Frame;

typedef struct Scope {
  Frame *frame;
  struct Scope *next;
  struct Scope *prev;
} Scope;

typedef struct State {
  Scope *scope_top;
  Scope *scope_cur;
  struct State *next;
  struct State *prev;
} State;

typedef struct {
  State *state_glob;
  State *state_cur;
} Interpreter;

// Function prototypes
Interpreter *interpret(Parser *parser);
void interpreter_destroy(Interpreter **interpreter);

// ERRORS
enum {
  ERR_OKAY,
  ERR_NULL_ARGS,
  ERR_INVALID_ARGS,
  ERR_MISMATCHED_NODE,
  ERR_UNKNOWN_NODE,
  ERR_CREATE_FAIL,
  ERR_INTERP_MISSING_COMPONENT,
  ERR_NODE_MISSING_COMPONENT,
  ERR_TODO
};

#endif // interpreter.h
