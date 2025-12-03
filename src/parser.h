#ifndef PARSER_H

#define PARSER_H

#include "tokeniser.h"

typedef enum { NodeProgram, NodeVarDecl, NodeVarAssign, NodeExpr } NodeType;

typedef enum { VarInteger, VarReal, VarBoolean, VarCharacter } VarType;

typedef enum { ExprInt, ExprVar, ExprOp } ExprType;

typedef enum { OpAdd, OpSubtract, OpDivide, OpMultiply, OpExponent, OpModulo, OpIntDiv } Op;

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
      ExprType type;

      union {
        int int_val;
        char *var_name;
        struct {
          Op op;
          struct ASTNode *left;
          struct ASTNode *right;
        } op;
      };
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
