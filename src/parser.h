#ifndef PARSER_H

#define PARSER_H

#include "tokeniser.h"

typedef enum { NodeProgram,
               NodeVarDecl,
               NodeVarAssign,
               NodeExpr,
               NodeBlock,
               NodeIf,
               NodeSend } NodeType;

typedef enum { VarInteger,
               VarReal,
               VarBoolean,
               VarCharacter } VarType;

typedef enum { ExprInt,
               ExprVar,
               ExprOp } ExprType;

typedef enum { OpAdd,
               OpSubtract,
               OpDivide,
               OpMultiply,
               OpExponent,
               OpModulo,
               OpIntDiv,
               OpEqual,
               OpNEqual,
               OpGreaterThan,
               OpGreaterThanEq,
               OpLessThan,
               OpLessThanEq,
} Op;

typedef struct ASTNode {
  NodeType type;
  union {
    // A program
    struct {
      struct ASTNode *block;
    } program;

    // A list of statements, in most other languages it would be a block of
    // code
    struct {
      size_t count;
      struct ASTNode **statements;
    } block;

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

    // If statement
    struct {
      struct ASTNode *condition;
      struct ASTNode *if_block;
      struct ASTNode *else_block;
    } if_stmt;

    // Send statement
    struct {
      struct ASTNode *expr;
      char *device_name;
    } send_stmt;
  };
} ASTNode;

typedef struct {
  ASTNode *root;
} Parser;

Parser *parse(Tokeniser *tokeniser);
void parser_destroy(Parser **parser);
void parser_dump(Parser *parser);

#endif
