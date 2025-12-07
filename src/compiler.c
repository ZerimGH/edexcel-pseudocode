#include "compiler.h"
#include "def.h"
#include <stdio.h>

int compile_node(ASTNode *node, Compiler *c);

static void indent(Compiler *c) {
  for(size_t i = 0; i < c->indent; i++)
    fprintf(c->out_file, "  ");
}

int compile_program(ASTNode *node, Compiler *c) {
  fprintf(c->out_file, "int main(void)");
  int block_status = compile_node(node->program.block, c);
  if(block_status) {
    PERROR("Failed to compile program's block.\n");
    return 1;
  }
  return 0;
}

int compile_block(ASTNode *node, Compiler *c) {
  if(!node->block.statements) {
    PERROR("Block node is missing statements.\n");
    return 1;
  }

  fprintf(c->out_file, " {\n");
  c->indent++;

  for(size_t i = 0; i < node->block.count; i++) {
    indent(c);
    int status = compile_node(node->block.statements[i], c);
    if(status) {
      PERROR("Failed to compile statement index %zu\n", i);
      return 1;
    }
  }

  c->indent--;
  indent(c);
  fprintf(c->out_file, "}\n");

  return 0;
}

static char *var_type_to_c(VarType t) {
  switch(t) {
  case VarInteger:
    return "int";
  case VarBoolean:
    return "int";
  case VarCharacter:
    return "char";
  case VarReal:
    return "float";
  default:
    return NULL;
  }
}

int compile_var_decl(ASTNode *node, Compiler *c) {
  if(!node->var_decl.id) {
    PERROR("Variable declaration node is missing an identifier.\n");
    return 1;
  }

  char *ctype = var_type_to_c(node->var_decl.type);
  if(!ctype) {
    PERROR("Unknown type\n");
    return 1;
  }

  fprintf(c->out_file, "%s %s;\n", ctype, node->var_decl.id);
  return 0;
}

int compile_var_assign(ASTNode *node, Compiler *c) {
  if(!node->var_assign.id) {
    PERROR("Variable assignment node is missing an identifier.\n");
    return 1;
  }

  fprintf(c->out_file, "%s = ", node->var_assign.id);
  int expr_status = compile_node(node->var_assign.expr, c);
  if(expr_status) {
    PERROR("Failed to compile variable assignment expression.\n");
    return expr_status;
  }

  fprintf(c->out_file, ";\n");

  return 0;
}

int compile_expr(ASTNode *node, Compiler *c) {
  switch(node->expr.type) {
  case ExprInt:
    fprintf(c->out_file, "(%d)", node->expr.int_val);
    return 0;

  case ExprVar:
    fprintf(c->out_file, "(%s)", node->expr.var_name);
    return 0;

  case ExprOp: {
    fprintf(c->out_file, "(");
    int ls = compile_expr(node->expr.op.left, c);
    if(ls) return ls;

    const char *op = NULL;
    switch(node->expr.op.op) {
    case OpAdd:
      op = "+";
      break;
    case OpSubtract:
      op = "-";
      break;
    case OpMultiply:
      op = "*";
      break;
    case OpDivide:
      op = "/";
      break;
    case OpModulo:
      op = "%";
      break;
    case OpIntDiv:
      op = "/";
      break;
    case OpExponent:
      op = "^";
      break;
    default:
      return 1;
    }

    fprintf(c->out_file, " %s ", op);

    int rs = compile_expr(node->expr.op.right, c);
    if(rs) return rs;

    fprintf(c->out_file, ")");
    return 0;
  }
  }
  return 1;
}

int compile_if(ASTNode *node, Compiler *c) {
  fprintf(c->out_file, "if");
  int expr_status = compile_node(node->if_stmt.condition, c);
  if(expr_status) {
    PERROR("Failed to compile if statement condition.\n");
    return 1;
  }

  int block_status = compile_node(node->if_stmt.if_block, c);
  if(block_status) {
    PERROR("Failed to compile if statement block.\n");
    return 1;
  }

  if(node->if_stmt.else_block) {
    indent(c);
    fprintf(c->out_file, "else");
    int else_status = compile_node(node->if_stmt.else_block, c);
    if(else_status) {
      PERROR("Failed to compile else statement block.\n");
      return 1;
    }
  }

  return 0;
}

int compile_node(ASTNode *node, Compiler *c) {
  if(!node) {
    PERROR("NULL node passed.\n");
    return 1;
  }

  int status = 0;
  switch(node->type) {
  case NodeProgram:
    status = compile_program(node, c);
    break;
  case NodeBlock:
    status = compile_block(node, c);
    break;
  case NodeVarDecl:
    status = compile_var_decl(node, c);
    break;
  case NodeVarAssign:
    status = compile_var_assign(node, c);
    break;
  case NodeExpr:
    status = compile_expr(node, c);
    break;
  case NodeIf:
    status = compile_if(node, c);
    break;
  default:
    PERROR("Unimplemented node type: %d\n", node->type);
    status = 1;
  }

  return status;
}

int compile(Parser *parser, FILE *out_file) {
  if(!parser) {
    PERROR("NULL parser passed.\n");
    return 1;
  }

  if(!out_file) {
    PERROR("NULL out_file passed.\n");
    return 1;
  }

  Compiler c = {
      .out_file = out_file,
      .indent = 0};

  int status = compile_node(parser->root, &c);
  if(status) {
    PERROR("Failed to compile: status %d\n", status);
  }
  return status;
}
