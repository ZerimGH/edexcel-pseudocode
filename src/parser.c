#include "parser.h"
#include "def.h"
#include "tokeniser.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERROR_LOC                                                                 \
  {                                                                                \
    Token *top = tokeniser_top(tokeniser);                                         \
    if(!top)                                                                       \
      PERROR("Error occured at line ?, char ?\n");                                 \
    else {                                                                         \
      PERROR("Error occured at line %zu, char %zu\n", top->line_no, top->char_no); \
    }                                                                              \
  }

// Convert a token type to a variable type
static VarType token_type_to_var_type(TokenType token_type) {
  switch(token_type) {
  case TokenInteger:
    return VarInteger;
  case TokenReal:
    return VarReal;
  case TokenBoolean:
    return VarBoolean;
  case TokenCharacter:
    return VarCharacter;
  default:
    return -1;
  }
}

// Check if a token type is a variable type
static int is_var_type(TokenType token_type) {
  return token_type_to_var_type(token_type) != -1;
}

// Create an AST node with a type
static ASTNode *node_create(NodeType type) {
  ASTNode *node = calloc(1, sizeof(ASTNode));
  if(!node) {
    PERROR("calloc() failed.\n");
    return NULL;
  }

  node->type = type;
  return node;
}

// Destroy an AST node
static void node_destroy(ASTNode **node) {
  if(!node) return;
  ASTNode *n = *node;
  if(!n) return;
  switch(n->type) {
  case NodeProgram:
    node_destroy(&n->program.block);
    break;
  case NodeVarDecl:
    if(n->var_decl.id) free(n->var_decl.id);
    break;
  case NodeVarAssign:
    if(n->var_assign.id) free(n->var_assign.id);
    node_destroy(&n->var_assign.expr);
    break;
  case NodeExpr:
    switch(n->expr.type) {
    case ExprOp:
      node_destroy(&n->expr.op.left);
      node_destroy(&n->expr.op.right);
      break;
    case ExprVar:
      if(n->expr.var_name) free(n->expr.var_name);
      n->expr.var_name = NULL;
      break;
    case ExprInt:
      // Nothing to free
      break;
    default:
      PERROR("UNIMPLEMENTED EXPRESSION DESTRUCTION!!!!!\n");
      break;
    }
    break;
  case NodeIf:
    node_destroy(&n->if_stmt.condition);
    node_destroy(&n->if_stmt.if_block);
    node_destroy(&n->if_stmt.else_block);
    break;
  case NodeWhile:
    node_destroy(&n->while_stmt.condition);
    node_destroy(&n->while_stmt.while_block);
    break;
  case NodeBlock:
    if(n->block.statements) {
      for(size_t i = 0; i < n->block.count; i++) {
        node_destroy(&n->block.statements[i]);
      }
      free(n->block.statements);
      n->block.statements = NULL;
      n->block.count = 0;
    }
    break;
  case NodeSend:
    node_destroy(&n->send_stmt.expr);
    if(n->send_stmt.device_name) free(n->send_stmt.device_name);
    n->send_stmt.device_name = NULL;
    break;
  default:
    PERROR("UNIMPLEMENTED NODE_DESTROY()!!!\n");
    break;
  }
  free(n);
  *node = NULL;
}

// Create a parser
static Parser *parser_create(void) {
  Parser *parser = malloc(sizeof(Parser));
  if(!parser) {
    PERROR("calloc() failed.\n");
    return NULL;
  }

  parser->root = NULL;

  return parser;
}

// Destroy a parser
void parser_destroy(Parser **parser) {
  if(!parser) return;
  Parser *p = *parser;
  if(!p) return;
  node_destroy(&p->root);
  free(p);
  *parser = NULL;
}

static NodeType detect_type(Tokeniser *tokeniser) {
  if(!tokeniser) return -1;
  Token *token = tokeniser_top(tokeniser);
  if(!token) return -1;

  if(is_var_type(token->type)) return NodeVarDecl;
  if(token->type == TokenSet) return NodeVarAssign;
  if(token->type == TokenIf) return NodeIf;
  if(token->type == TokenWhile) return NodeWhile;
  if(token->type == TokenSend) return NodeSend;

  // Couldn't detect type
  return -1;
}

// Parse a variable declaration
static ASTNode *parse_var_decl(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *type_tok = tokeniser_expect(tokeniser, 4, TokenInteger, TokenReal, TokenBoolean, TokenCharacter);
  if(!type_tok) {
    PERROR("Expected variable type\n");
    PERROR_LOC
    return NULL;
  }

  VarType type = token_type_to_var_type(type_tok->type);
  Token *ident_tok = tokeniser_expect(tokeniser, 1, TokenIdentifier);
  if(!ident_tok) {
    PERROR("Expected identifier.\n");
    PERROR_LOC
    return NULL;
  }

  ASTNode *node = node_create(NodeVarDecl);
  if(!node) {
    PERROR("Failed to create node.\n");
    return NULL;
  }

  char *id = strdup(ident_tok->value);
  if(!id) {
    PERROR("strdup() failed.\n");
    node_destroy(&node);
    return NULL;
  }

  node->var_decl.type = type;
  node->var_decl.id = id;
  return node;
}

// OKay
// What are all tokens that can be in an expression

#define VALUE_TYPES TokenIdentifier, TokenIntLit
#define OPERATOR_TOKS TokenAdd, TokenSubtract, TokenDivide, TokenMultiply, TokenExponent, TokenModulo, TokenIntDiv, TokenEqualTo, TokenNEqualTo, TokenGreaterThan, TokenGreaterThanEq, TokenLessThan, TokenLessThanEq
#define OPERATOR_PRECEDENCES 1, 1, 2, 2, 3, 2, 2, 0, 0, 0, 0, 0, 0
#define OPERATOR_OPS OpAdd, OpSubtract, OpDivide, OpMultiply, OpExponent, OpModulo, OpIntDiv, OpEqual, OpNEqual, OpGreaterThan, OpGreaterThanEq, OpLessThan, OpLessThanEq

static Token *consume_expr_tok(Tokeniser *tokeniser) {
  // Stupid hack
  TokenType toks[] = {VALUE_TYPES, OPERATOR_TOKS};
  size_t num = sizeof(toks) / sizeof(toks[0]);
  return tokeniser_expect(tokeniser, num, VALUE_TYPES, OPERATOR_TOKS);
}

static int is_op_tok(Token *tok) {
  TokenType op_toks[] = {OPERATOR_TOKS};
  for(size_t i = 0; i < sizeof(op_toks) / sizeof(op_toks[0]); i++) {
    if(tok->type == op_toks[i]) return 1;
  }
  return 0;
}

static int is_value_tok(Token *tok) {
  TokenType value_toks[] = {VALUE_TYPES};
  for(size_t i = 0; i < sizeof(value_toks) / sizeof(value_toks[0]); i++) {
    if(tok->type == value_toks[i]) return 1;
  }
  return 0;
}

static int token_precedence(Token *tok) {
  TokenType op_toks[] = {OPERATOR_TOKS};
  int op_prec[] = {OPERATOR_PRECEDENCES};
  for(size_t i = 0; i < sizeof(op_toks) / sizeof(op_toks[0]); i++) {
    if(tok->type == op_toks[i]) return op_prec[i];
  }
  return -1;
}

static Op get_expr_op_from_token(Token *tok) {
  TokenType op_toks[] = {OPERATOR_TOKS};
  Op ops[] = {OPERATOR_OPS};
  for(size_t i = 0; i < sizeof(op_toks) / sizeof(op_toks[0]); i++) {
    if(tok->type == op_toks[i]) return ops[i];
  }
  return -1;
}

/*
https://en.wikipedia.org/wiki/Shunting_yard_algorithm#The_algorithm_in_detail
while there are tokens to be read:
    read a token
    if the token is:
    - a number or variable:
        put it into the output queue
    - an operator o1:
        while (
            there is an operator o2 at the top of the operator stack which is not a left parenthesis,
            and (o2 has greater precedence than o1 or (o1 and o2 have the same precedence and o1 is left-associative))
        ):
            pop o2 from the operator stack into the output queue
        push o1 onto the operator stack
    - a left parenthesis (i.e. "("):
        push it onto the operator stack
    - a right parenthesis (i.e. ")"):
        while the operator at the top of the operator stack is not a left parenthesis:
            {assert the operator stack is not empty}
            pop the operator from the operator stack into the output queue
        {assert there is a left parenthesis at the top of the operator stack}
        pop the left parenthesis from the operator stack and discard it
while there are tokens on the operator stack:
    {assert the operator on top of the stack is not a (left) parenthesis}
    pop the operator from the operator stack onto the output queue

    why is it this complicated holy
*/

typedef struct {
  Token **toks;
  size_t alloced;
  size_t count;
} TokenQueue;

static TokenQueue *token_queue_create(void) {
  TokenQueue *queue = calloc(1, sizeof(TokenQueue));
  if(!queue) {
    PERROR("calloc() failed.\n");
    return NULL;
  }

  queue->alloced = 1024;
  queue->count = 0;
  queue->toks = calloc(queue->alloced, sizeof(Token *));
  if(!queue->toks) {
    PERROR("calloc() failed.\n");
    free(queue);
    return NULL;
  }

  return queue;
}

static void token_queue_destroy(TokenQueue **queue) {
  if(!queue) return;
  TokenQueue *q = *queue;
  if(!q) return;
  // Don't free the individual tokens, they are still owned by the tokeniser.
  if(q->toks) free(q->toks);
  q->toks = NULL;
  q->alloced = 0;
  q->count = 0;
  free(q);
  *queue = NULL;
}

static int token_queue_push(TokenQueue *queue, Token *tok) {
  if(!queue || !tok) return 1;
  if(queue->count >= queue->alloced) {
    queue->alloced *= 2;
    Token **new = realloc(queue->toks, sizeof(Token *) * queue->alloced);
    if(!new) {
      PERROR("realloc() failed.\n");
      queue->alloced /= 2;
      return 1;
    }
    queue->toks = new;
  }
  queue->toks[queue->count++] = tok;
  return 0;
}

static Token *token_queue_pop(TokenQueue *queue) {
  if(!queue) return NULL;
  if(!queue->toks) return NULL;
  if(queue->count == 0) return NULL;
  return (queue->toks[--queue->count]);
}

static Token *token_queue_pop_front(TokenQueue *queue) {
  if(!queue || !queue->count) return NULL;
  Token *tok = queue->toks[0];
  memmove(queue->toks, queue->toks + 1, sizeof(Token *) * (queue->count - 1));
  queue->count--;
  return tok;
}

static ASTNode *make_int_lit(char *val) {
  if(!val) return NULL;
  int int_val = atoi(val);
  ASTNode *node = node_create(NodeExpr);
  if(!node) {
    PERROR("Failed to create node.\n");
    return NULL;
  }

  node->expr.type = ExprInt;
  node->expr.int_val = int_val;
  return node;
}

static ASTNode *make_var(char *val) {
  if(!val) return NULL;
  ASTNode *node = node_create(NodeExpr);
  if(!node) {
    PERROR("Failed to create node.\n");
    return NULL;
  }

  node->expr.type = ExprVar;
  node->expr.var_name = strdup(val);
  return node;
}

ASTNode *create_value_node(Token *tok) {
  if(!tok) return NULL;
  if(!is_value_tok(tok)) return NULL;
  switch(tok->type) {
  case TokenIntLit:
    return make_int_lit(tok->value);
  case TokenIdentifier:
    return make_var(tok->value);
  default:
    PERROR("Unknown type %d.\n", tok->type);
    return NULL;
  }
}

ASTNode *create_expr_node(ASTNode *a, ASTNode *b, Token *op_tok) {
  if(!a || !b || !op_tok) return NULL;

  Op op = get_expr_op_from_token(op_tok);
  if(op == -1) {
    PERROR("Invalid operation.\n");
    return NULL;
  }

  ASTNode *node = node_create(NodeExpr);
  if(!node) {
    PERROR("Failed to create node.\n");
    return NULL;
  }

  node->expr.type = ExprOp;
  node->expr.op.op = op;
  node->expr.op.left = a;
  node->expr.op.right = b;
  return node;
}

// Parse an expression
// TODO: Make this function memory safe.
// RIght now, it doesnt free all memory on failure (value queue)
static ASTNode *parse_expr(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *tok = NULL;

  TokenQueue *output_queue = token_queue_create();
  if(!output_queue) {
    PERROR("Failed to allocate output queue.\n");
    return NULL;
  }

  TokenQueue *op_queue = token_queue_create();
  if(!op_queue) {
    PERROR("Failed to allocate operator queue.\n");
    token_queue_destroy(&output_queue);
    return NULL;
  }

  while((tok = consume_expr_tok(tokeniser))) {
    if(is_value_tok(tok)) {
      // put into output queue
      if(token_queue_push(output_queue, tok) != 0) {
        PERROR("Failed to append value to output queue.\n");
        goto err;
      }
    } else if(is_op_tok(tok)) {
      // while top of op stack has greater precedence: pop from op stack to
      //  output
      Token *popped = token_queue_pop(op_queue);
      while(popped && token_precedence(popped) >= token_precedence(tok)) {
        if(token_queue_push(output_queue, popped) != 0) {
          PERROR("Failed to push operator to output queue.\n");
          goto err;
        }
        popped = token_queue_pop(op_queue);
      }
      if(popped) {
        if(token_queue_push(op_queue, popped) != 0) {
          PERROR("Failed to return operator to operator queue.\n");
          goto err;
        }
      }
      // push into op stack
      if(token_queue_push(op_queue, tok) != 0) {
        PERROR("Failed to push operator to operator queue.\n");
        goto err;
      }
    }
    // TODO: parenthesis
    else {
      PERROR("huh\n");
      PERROR_LOC
      continue;
    }
  }

  // while there are tokens on the operator stack:
  //   pop the operator from the operator stack onto the output queue
  Token *op_tok = token_queue_pop(op_queue);
  while(op_tok) {
    if(token_queue_push(output_queue, op_tok) != 0) {
      PERROR("Failed to push remaining operator to output queue.\n");
      goto err;
    }
    op_tok = token_queue_pop(op_queue);
  }

  token_queue_destroy(&op_queue);

  // OKay
  // now the algorithm is something like:
  // while there are values in the otuput queue:
  //   if the value is a value:
  //     create an expression node with that value
  //     push to value stack
  //   if the value is an operator:
  //     pop value_b from value stack
  //     pop value_a from value stack
  //     create an expression node of (value_a op value_b)
  //     push to value stack
  // final expression node = pop from value stack

  // IDC fixed size queue
  ASTNode *value_queue[256] = {0};
  size_t count = 0;

  Token *popped = token_queue_pop_front(output_queue);
  // while there are values in the otuput queue:
  while(popped) {
    // if the value is a value:
    if(is_value_tok(popped)) {
      // create an expression node with that value
      ASTNode *value_node = create_value_node(popped);
      // push to value stack
      if(count < 256)
        value_queue[count++] = value_node;
      else {
        PERROR("Stack overflow!!\n");
        node_destroy(&value_node);
        goto err;
      }
    } else if(is_op_tok(popped)) {
      // pop value_b from value stack
      ASTNode *value_b = count > 0 ? value_queue[--count] : NULL;
      // pop value_a from value stack
      ASTNode *value_a = count > 0 ? value_queue[--count] : NULL;
      if(!value_b || !value_a) {
        if(value_b) node_destroy(&value_b);
        if(value_a) node_destroy(&value_a);
        PERROR("Failed to get value from value queue.\n");
        PERROR_LOC
        goto err;
      }
      // create an expression node of (value_a op value_b)
      ASTNode *expr_node = create_expr_node(value_a, value_b, popped);
      // push to value stack
      if(count < 256)
        value_queue[count++] = expr_node;
      else {
        node_destroy(&expr_node);
        PERROR("Stack overflow!!\n");
        goto err;
      }
    } else {
      PERROR("huh!!\n");
      goto err;
    }
    popped = token_queue_pop_front(output_queue);
  }

  token_queue_destroy(&output_queue);

  // final expression node = pop from value stack

  if(count != 1) {
    PERROR("Too many values left in value queue.\n");
    PERROR_LOC
    for(size_t i = 0; i < count; i++) {
      node_destroy(&value_queue[i]);
    }
    goto err;
  }

  return value_queue[0];

err:
  token_queue_destroy(&output_queue);
  token_queue_destroy(&op_queue);
  return NULL;
}

// Parse a variable assignment
static ASTNode *parse_var_assign(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *set_tok = tokeniser_expect(tokeniser, 1, TokenSet);
  if(!set_tok) {
    PERROR("Expected SET\n");
    return NULL;
  }

  Token *id_tok = tokeniser_expect(tokeniser, 1, TokenIdentifier);
  if(!id_tok) {
    PERROR("Expected identifier\n");
    return NULL;
  }
  char *id = strdup(id_tok->value);
  if(!id) {
    PERROR("strdup() failed.\n");
    return NULL;
  }

  Token *to_tok = tokeniser_expect(tokeniser, 1, TokenTo);
  if(!to_tok) {
    PERROR("Expected TO\n");
    free(id);
    return NULL;
  }

  // Expression
  ASTNode *expr_node = parse_expr(tokeniser);
  if(!expr_node) {
    PERROR("Failed to parse expression.\n");
    free(id);
    return NULL;
  }

  ASTNode *node = node_create(NodeVarAssign);
  if(!node) {
    PERROR("Failed to create node.\n");
    node_destroy(&expr_node);
    free(id);
    return NULL;
  }

  node->var_assign.id = id;
  node->var_assign.expr = expr_node;

  return node;
}

static ASTNode *parse_statement(Tokeniser *tokeniser);

ASTNode *parse_until(Tokeniser *tokeniser, size_t n, ...) {
  if(!tokeniser) return NULL;

  size_t allocated = 16;
  size_t count = 0;
  ASTNode **out = malloc(sizeof(ASTNode *) * allocated);
  if(!out) return NULL;

  while(1) {
    if(tokeniser_done(tokeniser)) {
      if(n == 0)
        break;
      else {
        PERROR("Unexpected end of tokens.\n");
        goto err;
      }
    }

    Token *tok = tokeniser_top(tokeniser);

    va_list args;
    va_start(args, n);
    int should_stop = 0;
    for(size_t i = 0; i < n; i++) {
      TokenType expected_type = va_arg(args, TokenType);
      if(tok->type == expected_type) {
        should_stop = 1;
        break;
      }
    }
    va_end(args);

    if(should_stop)
      break;

    ASTNode *statement = parse_statement(tokeniser);
    if(!statement) {
      if(n == 0 && tokeniser_done(tokeniser)) break;
      PERROR("Failed to parse a statement.\n");
      PERROR_LOC
      goto err;
    }

    if(count >= allocated) {
      allocated *= 2;
      ASTNode **new = realloc(out, sizeof(ASTNode *) * allocated);
      if(!new) {
        PERROR("realloc() failed.\n");
        node_destroy(&statement);
        goto err;
      }
      out = new;
    }

    out[count++] = statement;
  }

  if(count == 0) {
    PERROR("Empty block\n");
    goto err;
  }

  ASTNode *node = node_create(NodeBlock);
  if(!node) {
    PERROR("node_create() failed.\n");
    goto err;
  }

  node->block.count = count;
  node->block.statements = out;

  return node;

err:
  for(size_t i = 0; i < count; i++)
    node_destroy(&out[i]);
  free(out);
  return NULL;
}

static ASTNode *parse_if(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;

  Token *if_tok = tokeniser_expect(tokeniser, 1, TokenIf);
  if(!if_tok) {
    PERROR("Expected IF\n");
    PERROR_LOC
    return NULL;
  }

  ASTNode *cond = NULL;
  ASTNode *if_block = NULL;
  ASTNode *else_block = NULL;

  cond = parse_expr(tokeniser);
  if(!cond) {
    PERROR("Failed to parse condition.\n");
    return NULL;
  }

  if(!tokeniser_expect(tokeniser, 1, TokenThen)) {
    PERROR("Expected THEN\n");
    PERROR_LOC
    goto err;
  }

  if_block = parse_until(tokeniser, 2, TokenEnd, TokenElse);
  if(!if_block) {
    PERROR("Failed to parse IF statements.\n");
    goto err;
  }

  Token *top = tokeniser_top(tokeniser);
  if(top->type == TokenElse) {
    tokeniser_expect(tokeniser, 1, TokenElse);
    else_block = parse_until(tokeniser, 1, TokenEnd);
    if(!else_block) {
      PERROR("Failed to parse ELSE statements.\n");
      goto err;
    }
  }

  if(!tokeniser_expect(tokeniser, 1, TokenEnd) ||
     !tokeniser_expect(tokeniser, 1, TokenIf)) {
    PERROR("Expected END IF\n");
    PERROR_LOC
    goto err;
  }

  ASTNode *node = node_create(NodeIf);
  if(!node) {
    PERROR("Failed to create node.\n");
    goto err;
  }

  node->if_stmt.condition = cond;
  node->if_stmt.if_block = if_block;
  node->if_stmt.else_block = else_block;

  return node;

err:
  node_destroy(&cond);
  node_destroy(&if_block);
  node_destroy(&else_block);

  return NULL;
}

// Parse a while loop
static ASTNode *parse_while(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;

  Token *while_tok = tokeniser_expect(tokeniser, 1, TokenWhile);
  if(!while_tok) {
    PERROR("Expected WHILE\n");
    PERROR_LOC
    return NULL;
  }

  ASTNode *cond = NULL;
  ASTNode *while_block = NULL;

  cond = parse_expr(tokeniser);
  if(!cond) {
    PERROR("Failed to parse condition.\n");
    return NULL;
  }

  if(!tokeniser_expect(tokeniser, 1, TokenDo)) {
    PERROR("Expected DO\n");
    PERROR_LOC
    goto err;
  }

  while_block = parse_until(tokeniser, 1, TokenEnd);
  if(!while_block) {
    PERROR("Failed to parse WHILE statements.\n");
    goto err;
  }

  if(!tokeniser_expect(tokeniser, 1, TokenEnd) ||
     !tokeniser_expect(tokeniser, 1, TokenWhile)) {
    PERROR("Expected END WHILE\n");
    PERROR_LOC
    goto err;
  }

  ASTNode *node = node_create(NodeWhile);
  if(!node) {
    PERROR("Failed to create node.\n");
    goto err;
  }

  node->while_stmt.condition = cond;
  node->while_stmt.while_block = while_block;

  return node;

err:
  node_destroy(&cond);
  node_destroy(&while_block);

  return NULL;
}

// Parse a SEND statement
static ASTNode *parse_send(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *send_tok = tokeniser_expect(tokeniser, 1, TokenSend);
  if(!send_tok) {
    PERROR("Expected SEND\n");
    PERROR_LOC
    return NULL;
  }

  ASTNode *expr = parse_expr(tokeniser);
  if(!expr) {
    PERROR("Failed to parse SEND statement's expression.\n");
    return NULL;
  }

  Token *to_tok = tokeniser_expect(tokeniser, 1, TokenTo);
  if(!to_tok) {
    PERROR("Expected TO\n");
    PERROR_LOC
    node_destroy(&expr);
    return NULL;
  }

  Token *id_tok = tokeniser_expect(tokeniser, 1, TokenIdentifier);
  if(!id_tok) {
    PERROR("Expected device identifier.\n");
    PERROR_LOC
    node_destroy(&expr);
    return NULL;
  }

  char *device_name = strdup(id_tok->value);
  if(!device_name) {
    PERROR("strdup() failed.\n");
    node_destroy(&expr);
    return NULL;
  }

  ASTNode *node = node_create(NodeSend);
  if(!node) {
    PERROR("node_create() failed.\n");
    node_destroy(&expr);
    return NULL;
  }

  node->send_stmt.expr = expr;
  node->send_stmt.device_name = device_name;
  return node;
}

// Parse a statement
static ASTNode *parse_statement(Tokeniser *tokeniser) {
  // Detect the node type
  NodeType type = detect_type(tokeniser);
  if(type == -1) {
    PERROR("Failed to detect statement type.\n");
    PERROR_LOC
    return NULL;
  }

  switch(type) {
  case NodeVarDecl:
    return parse_var_decl(tokeniser);
  case NodeVarAssign:
    return parse_var_assign(tokeniser);
  case NodeIf:
    return parse_if(tokeniser);
  case NodeWhile:
    return parse_while(tokeniser);
  case NodeSend:
    return parse_send(tokeniser);
  default:
    PERROR("Unimplemented node type %d\n", type);
    return NULL;
  }
}

static ASTNode *parse_program(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  ASTNode *block = parse_until(tokeniser, 0);
  if(!block) {
    PERROR("Failed to parse program.\n");
    return NULL;
  }

  ASTNode *node = node_create(NodeProgram);
  if(!node) {
    PERROR("node_create() failed.\n");
    node_destroy(&block);
    return NULL;
  }

  node->program.block = block;
  return node;
}

// Construct an AST from a tokeniser's output
Parser *parse(Tokeniser *tokeniser) {
  if(!tokeniser || tokeniser->status != 0) {
    PERROR("Invalid tokeniser passed.\n");
    return NULL;
  }

  // Create the parser
  Parser *parser = parser_create();
  if(!parser) {
    PERROR("Failed to create parser.\n");
    return NULL;
  }

  parser->root = parse_program(tokeniser);
  if(!parser->root) {
    PERROR("Failed to parse program\n");
    parser_destroy(&parser);
    return NULL;
  }

  return parser;
}

static inline void print_indent(size_t indent) {
  for(size_t i = 0; i < indent; i++) putchar(' ');
}

static const char *var_type_to_str(VarType type) {
  switch(type) {
  case VarInteger:
    return "VarInteger";
  case VarReal:
    return "VarReal";
  case VarBoolean:
    return "VarBoolean";
  case VarCharacter:
    return "VarCharacter";
  default:
    return "?";
  }
}

static const char *expr_op_to_str(Op op) {
  switch(op) {
  case OpAdd:
    return "OpAdd";
  case OpSubtract:
    return "OpSubtract";
  case OpDivide:
    return "OpDivide";
  case OpMultiply:
    return "OpMultiply";
  case OpExponent:
    return "OpExponent";
  case OpModulo:
    return "OpModulo";
  case OpIntDiv:
    return "OpIntDiv";
  default:
    return "?";
  }
}

#define NODE_PRINTF(fmt, ...) \
  print_indent(indent);       \
  printf(fmt, ##__VA_ARGS__);

static void node_print(ASTNode *node, size_t indent, int indent_head) {
  if(indent_head) {
    print_indent(indent);
  }
  if(!node) {
    printf("(null)\n");
    return;
  }
  printf("(ASTNode) {\n");
  switch(node->type) {
  case NodeProgram:
    NODE_PRINTF("  NodeType type = NodeProgram\n");
    NODE_PRINTF("  program.block = ");
    node_print(node->program.block, indent + 2, 0);
    break;
  case NodeVarDecl:
    NODE_PRINTF("  NodeType type = NodeVarDecl\n");
    NODE_PRINTF("  var_decl.type = %s\n", var_type_to_str(node->var_decl.type));
    NODE_PRINTF("  var_decl.id = \"%s\"\n", node->var_decl.id);
    break;
  case NodeVarAssign:
    NODE_PRINTF("  NodeType type = NodeVarAssign\n");
    NODE_PRINTF("  var_assign.id = \"%s\"\n", node->var_assign.id);
    NODE_PRINTF("  var_assign.expr = ");
    node_print(node->var_assign.expr, indent + 2, 0);
    break;
  case NodeExpr:
    NODE_PRINTF("  NodeType type = NodeExpr\n");
    switch(node->expr.type) {
    case ExprOp:
      NODE_PRINTF("  expr.type = ExprOp\n");
      NODE_PRINTF("  expr.op.op = %s\n", expr_op_to_str(node->expr.op.op));
      NODE_PRINTF("  expr.op.left = ");
      node_print(node->expr.op.left, indent + 2, 0);
      NODE_PRINTF("  expr.op.right = ");
      node_print(node->expr.op.right, indent + 2, 0);
      break;
    case ExprInt:
      NODE_PRINTF("  expr.type = ExprInt\n");
      NODE_PRINTF("  expr.int_val = %d\n", node->expr.int_val);
      break;
    case ExprVar:
      NODE_PRINTF("  expr.type = ExprVar\n");
      NODE_PRINTF("  expr.var_name = %s\n", node->expr.var_name);
      break;
    default:
      NODE_PRINTF("?\n");
      break;
    }
    break;
  case NodeIf:
    NODE_PRINTF("  NodeType type = NodeIf\n");
    NODE_PRINTF("  if_stmt.condition = ");
    node_print(node->if_stmt.condition, indent + 2, 0);
    NODE_PRINTF("  if_stmt.if_block = ");
    node_print(node->if_stmt.if_block, indent + 2, 0);
    NODE_PRINTF("  if_stmt.else_block = ");
    node_print(node->if_stmt.else_block, indent + 2, 0);
    break;
  case NodeWhile:
    NODE_PRINTF("  NodeType type = NodeWhile\n");
    NODE_PRINTF("  while_stmt.condition = ");
    node_print(node->while_stmt.condition, indent + 2, 0);
    NODE_PRINTF("  while_stmt.while_block = ");
    node_print(node->while_stmt.while_block, indent + 2, 0);
    break;
  case NodeBlock:
    NODE_PRINTF("  NodeType type = NodeBlock\n");
    NODE_PRINTF("  block.count = %zu\n", node->block.count);
    if(node->block.statements) {
      NODE_PRINTF("  block.statements = {\n");
      for(size_t i = 0; i < node->block.count; i++) {
        node_print(node->block.statements[i], indent + 4, 1);
      }
      NODE_PRINTF("  }\n");
    } else {
      NODE_PRINTF("  block.statements = (null)\n");
    }
    break;
  case NodeSend:
    NODE_PRINTF("  NodeType type = NodeSend\n");
    NODE_PRINTF("  send_stmt.expr = ");
    node_print(node->send_stmt.expr, indent + 2, 0);
    NODE_PRINTF("  send_stmt.device_name = \"%s\"\n", node->send_stmt.device_name);
    break;
  default:
    NODE_PRINTF("?\n");
    break;
  }
  print_indent(indent);
  printf("}\n");
}

void parser_dump(Parser *parser) {
  if(!parser) {
    printf("(null)\n");
    return;
  }

  printf("(Parser) {\n");
  printf("  ASTNode *root = {\n");
  node_print(parser->root, 4, 1);
  printf("  }\n");
  printf("}\n");
}
