#include "interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "def.h"

// FRAME NODE IMPLEMENTATION
// Create a frame node
static FrameNode *frame_node_create(char *id) {
  FrameNode *f = malloc(sizeof(FrameNode));
  if(!f) {
    PERROR("malloc() failed.\n");
    return NULL;
  }

  f->var = (Variable) {
    .type = -1,
    .int_val = 0 
  };

  f->id = strdup(id);
  if(!f->id) {
    PERROR("strdup() failed.\n");
    free(f);
    return NULL;
  }
  f->next = NULL;
  return f;
}

// Destroy a frame node
static void frame_node_destroy(FrameNode **node) {
  if(!node) return;
  FrameNode *n = *node;
  if(!n) return;
  FrameNode *f = n;
  while(f) {
    FrameNode *next = f->next;
    if(f->id) free(f->id);
    f->id = NULL;
    f->next = NULL;
    free(f);
    f = next;
  }
  *node = NULL;
}

// FRAME IMPLEMENTATION
// Create a frame
static Frame *frame_create(void) {
  Frame *f = malloc(sizeof(Frame));  
  if(!f) {
    PERROR("malloc() failed.\n");
    return NULL;
  }
  for(size_t i = 0; i < NUM_BUCKETS; i++) {
    f->buckets[i] = NULL;
  }

  return f;
}

// Destroy a frame
static void frame_destroy(Frame **frame) {
  if(!frame) return;
  Frame *f = *frame;
  if(!f) return;
  for(size_t i = 0; i < NUM_BUCKETS; i++) {
    frame_node_destroy(&f->buckets[i]);
  }
  free(f);
  *frame = NULL;
}

// Hash function for a string
// from: http://www.cse.yorku.ca/~oz/hash.html
static inline uint32_t hash_str(const char *str) {
  uint64_t hash = 0;
  int c;

  while((c = *(str++))) {
    hash = c + (hash << 6) + (hash << 16) - hash;
  }

  return (uint32_t) hash;
}

// Insert a variable to a frame
static int frame_insert(Frame *frame, FrameNode *node) {
  if(!node || !frame) {
    PERROR("Invalid node or frame passed.\n");
    return 1;
  }

  uint32_t idx = hash_str(node->id) % NUM_BUCKETS;

  // If bucket is empty, insert at start 
  FrameNode *n = frame->buckets[idx];
  FrameNode *p = NULL;
  if(!n) {
    frame->buckets[idx] = node;
    return ERR_OKAY;
  } else {
    // Otherwise, if it does not exist already, append to the end
    while(n) {
      if(strcmp(node->id, n->id) == 0) {
        PERROR("Variable already exists in frame.\n");
        return 1;
      }
      p = n;
      n = n->next;
    }
    p->next = node;
    return ERR_OKAY;
  }
}

// Lookup a variable in a frame
static FrameNode *frame_lookup(Frame *frame, char *id) {
  if(!frame || !id) {
    PERROR("Invalid frame or id string passed.\n");
    return NULL;
  }

  uint32_t idx = hash_str(id) % NUM_BUCKETS;

  FrameNode *n = frame->buckets[idx];
  if(!n) {
    PERROR("Variable \"%s\" not found: bucket was empty.\n", id);
    return NULL;
  }

  while(n) {
    if(strcmp(n->id, id) == 0) return n;
    n = n->next;
  }

  PERROR("Variable \"%s\" not found: bucket did not contain id.\n", id);
  return NULL;
}

// SCOPE IMPLEMENTATION
// Create a scope
static Scope *scope_create(void) {
  Scope *s = malloc(sizeof(Scope));
  if(!s) {
    PERROR("malloc() failed.\n");
    return NULL;
  }

  s->frame = frame_create();
  if(!s->frame) {
    PERROR("frame_create() failed.\n");
    free(s);
    return NULL;
  }
  s->next = NULL;
  s->prev = NULL;
  return s;
}

// Destroy a scope
static void scope_destroy(Scope **scope) {
  if(!scope) return; 
  Scope *s = *scope;
  if(!s) return;
  Scope *c = s;
  while(c) {
    Scope *next = c->next;
    if(c->prev) {
      c->prev->next = NULL;
    }
    frame_destroy(&c->frame);
    free(c);
    c = next;
  }
  *scope = NULL;
}

// Append a new scope to a scope
static int scope_push_scope(Scope *scope) {
  if(!scope) { 
    PERROR("NULL Scope passed!\n");
    return ERR_NULL_ARGS;
  }

  Scope *new = scope_create();
  if(!new) {
    PERROR("Failed to create a new scope.\n");
    return ERR_CREATE_FAIL;
  }

  if(scope->next) scope_destroy(&scope->next);
  scope->next = new;
  return ERR_OKAY;
}

// Declare a variable within a scope
static int scope_declare(Scope *scope, char *id, VarType type) {
  Frame *frame = scope->frame;
  if(!frame) {
    PERROR("Failed to declare variable \"%s\"\n: Scope is missing a frame!\n", id);
    return ERR_INTERP_MISSING_COMPONENT;
  }

  FrameNode *fn = frame_node_create(id);
  if(!fn) {
    PERROR("frame_node_create() failed.\n");
    return ERR_CREATE_FAIL;
  }

  fn->var = var_new(type);
  if(fn->var.type == -1) {
    PERROR("Failed to initialise variable \"%s\".\n", id);
  }

  int insert_status = frame_insert(frame, fn);  
  if(insert_status) {
    PERROR("Failed to declare variable \"%s\": frame_insert() failed.\n", id);
    frame_node_destroy(&fn);
    return insert_status;
  }

  return ERR_OKAY;
}

// STATE IMPLEMENTATION
// Create a state
static State *state_create(void) {
  State *state = malloc(sizeof(State));
  if(!state) {
    PERROR("malloc() failed.\n");
    return NULL;
  }

  state->scope_top = scope_create();
  if(!state->scope_top) {
    PERROR("scope_create() failed.\n");
    free(state);
    return NULL;
  }

  state->scope_cur = state->scope_top;

  state->next = NULL;
  state->prev = NULL;
  return state;
}

// Destroy a state 
static void state_destroy(State **state) {
  if(!state) return; 
  State *s = *state;
  if(!s) return;
  State *t = s;
  while(t) {
    State *next = t->next;
    scope_destroy(&t->scope_top);
    if(t->prev) t->prev->next = NULL;
    free(t);
    t = next;
  }

  *state = NULL;
}

// Push a new scope for a state
static int state_push_scope(State *state) {
  if(!state->scope_cur) {
    PERROR("State is missing a scope!\n");
    return ERR_INTERP_MISSING_COMPONENT;
  }

  int push_status = scope_push_scope(state->scope_cur);
  if(push_status) {
    PERROR("Failed to push a new scope\n");
    return push_status;
  }

  state->scope_cur = state->scope_cur->next;
  return ERR_OKAY;
}

// Declare a variable in a state
static int state_declare(State *state, char *id, VarType type) {
  if(!state->scope_cur) {
    PERROR("State is missing a scope!\n");
    return ERR_INTERP_MISSING_COMPONENT;
  } 

  int status = scope_declare(state->scope_cur, id, type);
  if(status) {
    PERROR("Failed to declare variable \"%s\".\n", id);
    return status;
  }
  return ERR_OKAY;
}

// INTERPRETER IMPLEMENTATION
// Create an interpreter
static Interpreter *interpreter_create(void) {
  Interpreter *i = malloc(sizeof(Interpreter));
  if(!i) {
    PERROR("malloc() failed.\n");
    return NULL;
  }

  i->state_glob = state_create();
  if(!i->state_glob) {
    PERROR("state_create() failed.\n");
    free(i);
    return NULL;
  }

  i->state_cur = i->state_glob;
  return i;
}

// Destroy an interpreter
void interpreter_destroy(Interpreter **interpreter) {
   if(!interpreter) return;
   Interpreter *i = *interpreter;
   if(!i) return;
   state_destroy(&i->state_glob);
   free(i);
   *interpreter = NULL;
}

// Push a new scope in the current state for the interpreter
int interpreter_push_scope(Interpreter *interpreter) {
  State *state = interpreter->state_cur;  
  if(!state) {
    PERROR("Interpreter is missing a state!\n");
    return ERR_INTERP_MISSING_COMPONENT;
  }

  int status = state_push_scope(state);
  if(status) {
    PERROR("Failed to push a new scope.\n");
    return status;
  }

  return ERR_OKAY;
}

// Declare a variable at the current scope
int interpreter_declare(Interpreter *interpreter, char *id, VarType type) {
  if(!interpreter) {
    PERROR("NULL interpreter passed.\n");
    return ERR_NULL_ARGS;
  }

  if(!id) {
    PERROR("NULL id passed.\n");
    return ERR_NULL_ARGS;
  }

  State *state = interpreter->state_cur;
  if(!state) {
    PERROR("Interpreter is missing a state!\n");
    return ERR_INTERP_MISSING_COMPONENT;
  }

  int status = state_declare(state, id, type);
  if(status) {
    PERROR("Failed to declare variable \"%s\".\n", id);
    return status;
  }

  return ERR_OKAY;
}

int interpret_node(Interpreter *interpreter, ASTNode *node);

// Interpret a block node
int interpret_block(Interpreter *interpreter, ASTNode *node) {
  if(!node->block.statements) {
    PERROR("Block node is missing statements!\n");
    return ERR_NODE_MISSING_COMPONENT;
  }

  // Push a new scope for the block
  int push_status = interpreter_push_scope(interpreter);
  if(push_status) {
    PERROR("Failed to push a new scope for the block.\n");
    return push_status;
  }

  for(size_t i = 0; i < node->block.count; i++) {
    int status = interpret_node(interpreter, node->block.statements[i]);
    if(status != 0) {
      PERROR("Failed to interpret statement index %zu.\n", i);
      return status;
    }
  } 

  return ERR_OKAY;
}

// Interpret a program node
int interpret_program(Interpreter *interpreter, ASTNode *node) {
  int block_status = interpret_node(interpreter, node->program.block);
  if(block_status) {
    PERROR("Failed to interpret program's block.\n");
    return block_status;
  }

  return ERR_OKAY;
}

int interpret_var_decl(Interpreter *interpreter, ASTNode *node) {
  if(!node->var_decl.id) {
    PERROR("Variable declaration node is missing an identifier!\n");
    return ERR_NODE_MISSING_COMPONENT;
  }

  int decl_status = interpreter_declare(interpreter, node->var_decl.id, node->var_decl.type);
  if(decl_status) {
    PERROR("Failed to declare variable \"%s\".\n", node->var_decl.id);
  }

  return ERR_OKAY;
}

// Interpret a node of the AST
int interpret_node(Interpreter *interpreter, ASTNode *node) {
  if(!interpreter) {
    PERROR("NULL interpreter passed.\n");
    return ERR_NULL_ARGS;
  } 

  if(!node) {
    PERROR("NULL node passed.\n");
    return ERR_NULL_ARGS;
  }

  int status = 0;
  switch(node->type) {
    case NodeProgram:
      status = interpret_program(interpreter, node);
      break;
    case NodeBlock:
      status = interpret_block(interpreter, node);
      break;
    case NodeVarDecl:
      status = interpret_var_decl(interpreter, node);
      break;
    default: 
      PERROR("Unknown node type %d\n", node->type);
      return ERR_UNKNOWN_NODE;
  }

  return status; 
}

// Interpret a program
Interpreter *interpret(Parser *parser) {
  if(!parser) {
    PERROR("NULL parser passed.\n");
    return NULL;
  }

  Interpreter *interpreter = interpreter_create();
  if(!interpreter) {
    PERROR("interpreter_create() failed.\n");
    return NULL;
  }

  int status = interpret_node(interpreter, parser->root);
  if(status != 0) {
    PERROR("Failed to interpret: status %d\n", status);
    interpreter_destroy(&interpreter);
    return NULL;
  }

  return interpreter;
}
