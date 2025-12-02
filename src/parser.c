#include "parser.h"
#include "def.h"
#include "tokeniser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if(n->program.nodes) {
      for(size_t i = 0; i < n->program.count; i++) {
        node_destroy(&n->program.nodes[i]);
      }
    }
    break;
  case NodeVarDecl:
    if(n->var_decl.id) free(n->var_decl.id);
    break;
  case NodeVarAssign:
    if(n->var_assign.id) free(n->var_assign.id);
    node_destroy(&n->var_assign.expr);
    break;
  case NodeExpr:
    // nothing to free for now
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
  Parser *parser = calloc(1, sizeof(Parser));
  if(!parser) {
    PERROR("calloc() failed.\n");
    return NULL;
  }

  parser->alloced = 1024;
  parser->count = 0;
  parser->statements = malloc(sizeof(ASTNode *) * parser->alloced);
  if(!parser->statements) {
    PERROR("malloc() failed.\n");
    free(parser);
    return NULL;
  }

  parser->root = NULL;
  parser->status = 0;

  return parser;
}

// Destroy a parser
void parser_destroy(Parser **parser) {
  if(!parser) return;
  Parser *p = *parser;
  if(!p) return;
  node_destroy(&p->root);
  if(p->statements) {
    for(size_t i = 0; i < p->count; i++) {
      node_destroy(&p->statements[i]);
    }
  }
  free(p);
  *parser = NULL;
}

void parser_append_statement(Parser *parser, ASTNode *statement) {
  if(!parser || !statement) {
    PERROR("Invalid arguments.\n");
    return;
  }
  if(parser->status != 0) {
    node_destroy(&statement);
    return;
  }

  if(parser->count >= parser->alloced) {
    parser->alloced *= 2;
    ASTNode **new = realloc(parser->statements, sizeof(ASTNode *) * parser->alloced);
    if(!new) {
      PERROR("realloc() failed.\n");
      parser->alloced /= 2;
      parser->status = 1;
      node_destroy(&statement);
      return;
    }
    parser->statements = new;
  }
  parser->statements[parser->count++] = statement;
}

static NodeType detect_type(Tokeniser *tokeniser) {
  if(!tokeniser) return -1;
  Token *token = tokeniser_top(tokeniser);;
  if(!token) return -1;

  // If it starts with a variable type, its a variable declaration
  if(is_var_type(token->type)) return NodeVarDecl;
  // If it starts with SET, its a variable asssignment
  if(token->type == TokenSet) return NodeVarAssign;

  // Couldn't detect type
  return -1;
}

// Parse a variable declaration
static ASTNode *parse_var_decl(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *type_tok = tokeniser_expect(tokeniser, 4, TokenInteger, TokenReal, TokenBoolean, TokenCharacter);
  if(!type_tok) {
    PERROR("Expected variable type\n");
    return NULL;
  }

  VarType type = token_type_to_var_type(type_tok->type);
  Token *ident_tok = tokeniser_expect(tokeniser, 1, TokenIdentifier);
  if(!ident_tok) {
    PERROR("Expected identifier.\n");
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

/*
https://en.wikipedia.org/wiki/Shunting_yard_algorithm#The_algorithm_in_detail 
while there are tokens to be read:
    read a token
    if the token is:
    - a number:
        put it into the output queue
    - a function:
        push it onto the operator stack 
    - an operator o1:
        while (
            there is an operator o2 at the top of the operator stack which is not a left parenthesis, 
            and (o2 has greater precedence than o1 or (o1 and o2 have the same precedence and o1 is left-associative))
        ):
            pop o2 from the operator stack into the output queue
        push o1 onto the operator stack
    - a ",":
        while the operator at the top of the operator stack is not a left parenthesis:
             pop the operator from the operator stack into the output queue
    - a left parenthesis (i.e. "("):
        push it onto the operator stack
    - a right parenthesis (i.e. ")"):
        while the operator at the top of the operator stack is not a left parenthesis:
            {assert the operator stack is not empty}
            pop the operator from the operator stack into the output queue
        {assert there is a left parenthesis at the top of the operator stack}
        pop the left parenthesis from the operator stack and discard it
        if there is a function token at the top of the operator stack, then:
            pop the function from the operator stack into the output queue
while there are tokens on the operator stack:
    {assert the operator on top of the stack is not a (left) parenthesis}
    pop the operator from the operator stack onto the output queue

    why is it this complicated holy
*/
static ASTNode *parse_expr(Tokeniser *tokeniser) {
  if(!tokeniser) return NULL;
  Token *lit_tok = tokeniser_expect(tokeniser, 1, TokenIntLit);
  if(!lit_tok) {
    PERROR("Expected integer literal\n");
    return NULL;
  }
  ASTNode *node = node_create(NodeExpr);
  if(!node) {
    PERROR("Failed to create node.\n");
    return NULL;
  }

  node->expr.integer_val = atoi(lit_tok->value);
  return node;
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

// Parse a statement
static ASTNode *parse_statement(Tokeniser *tokeniser) {
  // Detect the node type
  NodeType type = detect_type(tokeniser);
  if(type == -1) {
    PERROR("Failed to detect statement type.\n");
    return NULL;
  }

  switch(type) {
  case NodeVarDecl:
    return parse_var_decl(tokeniser);
  case NodeVarAssign:
    return parse_var_assign(tokeniser);
  default:
    PERROR("Unimplemented node type %d\n", type);
    return NULL;
  }
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

  // Parse statements until error or end
  while(!tokeniser_done(tokeniser)) {
    ASTNode *stmt = parse_statement(tokeniser);
    if(!stmt) {
      PERROR("Failed to parse a statement.\n");
      parser_destroy(&parser);
      return NULL;
    }
    parser_append_statement(parser, stmt);
  }

  // Check for error
  if(parser->status != 0) {
    PERROR("Failed to parse tokens.\n");
    parser_destroy(&parser);
    return NULL;
  }

  // Create root node
  parser->root = node_create(NodeProgram);
  if(!parser->root) {
    PERROR("Failed to create root node.\n");
    parser_destroy(&parser);
    return NULL;
  }

  // Move statements to root node
  parser->root->program.nodes = parser->statements;
  parser->statements = NULL;
  parser->root->program.count = parser->count;
  parser->count = 0;

  return parser;
}

static inline void print_indent(size_t indent) {
  for(size_t i = 0; i < indent; i++) putchar(' ');
}

static const char *var_type_to_str(VarType type) {
  switch(type) {
    case VarInteger: return "VarInteger";
    case VarReal: return "VarReal";
    case VarBoolean: return "VarBoolean";
    case VarCharacter: return "VarCharacter";
    default: return "?";
  }
}

static void node_print(ASTNode *node, size_t indent, int indent_head) {
  if(indent_head) { print_indent(indent); } printf("(ASTNode) {\n");
  switch(node->type) {
    case NodeProgram:
      print_indent(indent); printf("  NodeType type = NodeProgram\n");
      print_indent(indent); printf("  program.nodes = {\n");
      for(size_t i = 0; i < node->program.count; i++) {
        node_print(node->program.nodes[i], indent + 4, 1);
      }
      print_indent(indent); printf("  }\n");
      print_indent(indent); printf("  program.count = %zu\n", node->program.count);
      break;
    case NodeVarDecl:
      print_indent(indent); printf("  NodeType type = NodeVarDecl\n");
      print_indent(indent); printf("  var_decl.type = %s\n", var_type_to_str(node->var_decl.type));
      print_indent(indent); printf("  var_decl.id = \"%s\"\n", node->var_decl.id);
      break;
    case NodeVarAssign:
      print_indent(indent); printf("  NodeType type = NodeVarAssign\n");
      print_indent(indent); printf("  var_assign.id = \"%s\"\n", node->var_assign.id);
      print_indent(indent); printf("  var_assign.expr = ");
      node_print(node->var_assign.expr, indent + 2, 0);
      break;
    case NodeExpr:
      print_indent(indent); printf("  NodeType type = NodeExpr\n");
      print_indent(indent); printf("  expr.integer_val = %d\n", node->expr.integer_val);
      break;
    default: print_indent(indent); printf("?\n"); break;
  }
  print_indent(indent); printf("}\n");
}

void parser_dump(Parser *parser) {
  if(!parser) {
    printf("(null)\n");
    return;
  }

  printf("(Parser) {\n");
  if(parser->statements) {
    printf("  ASTNode **statements = {\n");
    for(size_t i = 0; i < parser->count; i++) {
      node_print(parser->statements[i], 4, 1);  
    }
    printf("  }\n");
  } else {
    printf("  ASTNode **statements = (null)\n");
  }
  printf("  size_t alloced = %zu\n", parser->alloced);
  printf("  size_t count = %zu\n", parser->count);
  printf("  int status = %d\n", parser->status);
  printf("  ASTNode *root = {\n");
  node_print(parser->root, 4, 1);
  printf("  }\n");
  printf("}\n");
}
