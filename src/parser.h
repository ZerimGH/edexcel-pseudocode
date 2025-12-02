#ifndef PARSER_H

#define PARSER_H

#include "tokeniser.h"

typedef enum { NodeProgram, NodeVarDecl, NodeVarAssign, NodeExpr } NodeType;

typedef enum { VarInteger, VarReal, VarBoolean, VarCharacter } VarType;

typedef struct ASTNode {
  NodeType type;
  union {
    // A program
    struct {
      struct ASTNode **nodes;
      size_t count;
    } program;

    // Variable declarations
    struct {
      VarType type;
      char *id;
    } var_decl;

    // Variable assignments
    struct {
      char *id;
      struct ASTNode *expr;
    } var_assign;

    // Expressions
    struct {
      // TODO: real expressions
      int integer_val;
    } expr;
  };
} ASTNode;

typedef struct {
  ASTNode **statements;
  size_t alloced;
  size_t count;

  int status;
  ASTNode *root;
} Parser;

Parser *parse(Tokeniser *tokeniser);
void parser_destroy(Parser **parser);
void parser_dump(Parser *parser);

#endif
