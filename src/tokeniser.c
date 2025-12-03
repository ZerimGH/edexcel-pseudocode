#include "tokeniser.h"
#include "def.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create a token with a type and value
static Token *token_create(TokenType type, char *value) {
  Token *tok = calloc(1, sizeof(Token));
  if(!tok) {
    PERROR("calloc() failed.\n");
    return NULL;
  }
  tok->type = type;
  tok->value = strdup(value);
  if(!tok->value) {
    PERROR("strdup() failed.\n");
    free(tok);
    return NULL;
  }

  return tok;
}

// Destroy a token
static void token_destroy(Token **token) {
  if(!token) return;
  Token *t = *token;
  if(!t) return;
  if(t->value) free(t->value);
  t->value = NULL;
  free(t);
  *token = NULL;
}

// Create a tokeniser
static Tokeniser *tokeniser_create(void) {
  Tokeniser *tokeniser = calloc(1, sizeof(Tokeniser));
  if(!tokeniser) {
    PERROR("calloc() failed.\n");
    return NULL;
  }
  tokeniser->count = 0;
  tokeniser->alloced = 1024;
  tokeniser->tokens = malloc(sizeof(Token *) * tokeniser->alloced);
  if(!tokeniser->tokens) {
    PERROR("malloc() failed.\n");
    free(tokeniser);
    return NULL;
  }
  tokeniser->status = 0;
  tokeniser->read = 0;
  return tokeniser;
}

// Destroy a tokeniser
void tokeniser_destroy(Tokeniser **tokeniser) {
  if(!tokeniser) return;
  Tokeniser *t = *tokeniser;
  if(!t) return;
  if(t->tokens) {
    for(size_t i = 0; i < t->count; i++) {
      token_destroy(&t->tokens[i]);
    }
    free(t->tokens);
  }
  t->tokens = NULL;
  free(t);
  *tokeniser = NULL;
}

// Append a token to a tokeniser's token list
static void tokeniser_append(Tokeniser *tokeniser, Token *token) {
  if(!tokeniser || !token) return;
  if(tokeniser->status != 0) return;

  if(tokeniser->count >= tokeniser->alloced) {
    tokeniser->alloced *= 2;
    Token **new = realloc(tokeniser->tokens, sizeof(Token *) * tokeniser->alloced);
    if(!new) {
      PERROR("realloc() failed.\n");
      free(tokeniser->tokens);
      tokeniser->tokens = NULL;
      tokeniser->status = 1;
      tokeniser->count = 0;
      tokeniser->alloced = 0;
    } else {
      tokeniser->tokens = new;
    }
  }
  tokeniser->tokens[tokeniser->count++] = token;
}

// Get the top token
Token *tokeniser_top(Tokeniser *tokeniser) {
  if(!tokeniser || tokeniser->status != 0) {
    PERROR("Invalid tokeniser.\n");
    return NULL;
  }

  if(tokeniser->read >= tokeniser->count) return NULL;
  Token *token = tokeniser->tokens[tokeniser->read];
  return token;
}

// Expect and consume one of any number of possible token types
Token *tokeniser_expect(Tokeniser *tokeniser, size_t num, ...) {
  if(!tokeniser || tokeniser->status != 0) {
    PERROR("Invalid tokeniser.\n");
    return NULL;
  }

  if(tokeniser->read >= tokeniser->count) return NULL;
  Token *token = tokeniser->tokens[tokeniser->read];
  if(!token) return NULL;

  va_list args;
  va_start(args, num);

  for(size_t i = 0; i < num; i++) {
    TokenType expected_type = va_arg(args, TokenType);
    if(token->type == expected_type) {
      va_end(args);
      tokeniser->read++;
      return token;
    }
  }

  va_end(args);
  return NULL;
}

// Return if every token has been read
int tokeniser_done(Tokeniser *tokeniser) {
  if(!tokeniser) return 1;
  if(tokeniser->status != 0) return 1;
  return tokeniser->read == tokeniser->count;
}

// Try to create a token of a keyword from a string
static Token *tokenise_keyword(char *src) {
  if(!src || *src == '\0' || isspace((unsigned char)*src)) return NULL;

  static const struct {
    TokenType type;
    char *value;
  } keywords[] = {
      {TokenInteger, "INTEGER"}, {TokenReal, "REAL"},     {TokenBoolean, "BOOLEAN"},  {TokenCharacter, "CHARACTER"}, {TokenSet, "SET"},       {TokenTo, "TO"},      {TokenAdd, "+"},
      {TokenSubtract, "-"},      {TokenDivide, "/"},      {TokenMultiply, "*"},       {TokenExponent, "^"},          {TokenModulo, "MOD"},    {TokenIntDiv, "DIV"}, {TokenEqualTo, "="},
      {TokenNEqualTo, "<>"},     {TokenGreaterThan, ">"}, {TokenGreaterThanEq, ">="}, {TokenLessThan, "<"},          {TokenLessThanEq, "<="},
  };

  size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);
  for(size_t i = 0; i < num_keywords; i++) {
    char *keyword_value = keywords[i].value;
    TokenType keyword_type = keywords[i].type;
    size_t keyword_len = strlen(keyword_value);

    if(strncmp(src, keyword_value, keyword_len) == 0) {
      Token *tok = token_create(keyword_type, keyword_value);
      if(!tok) {
        PERROR("Failed to allocate a token.\n");
        return NULL;
      }
      return tok;
    }
  }
  return NULL;
}

// Helper function to check if a character is valid for an identifier
static int is_ident_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

// Try to create a token of an identifier from a string
static Token *tokenise_ident(char *src) {
  if(!src || *src == '\0' || isspace((unsigned char)*src)) return NULL;
  if(!isalpha((unsigned char)*src))
    return NULL; // An identifier must start with an
                 // alphabetic character
  // Consume characters while valid identifier chars
  size_t ident_len = 1;
  while(is_ident_char(src[ident_len])) ident_len++;

  // Return a token with the identifier value
  char before = src[ident_len];
  src[ident_len] = '\0';
  Token *tok = token_create(TokenIdentifier, src);
  src[ident_len] = before;
  if(!tok) {
    PERROR("Failed to allocate a token.\n");
    return NULL;
  }
  return tok;
}

// Helper function to check if a character is a digit
static int is_digit(char c) {
  return c >= '0' && c <= '9';
}

// Try to create a token of an integer literal from a string
static Token *tokenise_int_lit(char *src) {
  if(!src || *src == '\0' || isspace((unsigned char)*src)) return NULL;
  if(!(is_digit(*src) || *src == '-'))
    return NULL; // An int literal must start with a
                 // digit or a negative sign
  int negative = *src == '-';
  size_t lit_len = 1;
  // Consume characters while digits
  while(is_digit(src[lit_len])) lit_len++;

  // If only a negative sign without digits, return NULL
  if(negative && lit_len == 1) return NULL;

  // Check that the next character is a space, newline, or end of string
  if(!isspace((unsigned char)src[lit_len]) && src[lit_len] != '\0') return NULL;

  // Return a token with the integer value
  char before = src[lit_len];
  src[lit_len] = '\0';
  Token *tok = token_create(TokenIntLit, src);
  src[lit_len] = before;
  if(!tok) {
    PERROR("Failed to allocate a token.\n");
    return NULL;
  }
  return tok;
}

// Tokenise a string
Tokeniser *tokenise(char *src) {
  if(!src) {
    PERROR("Invalid arguments.\n");
    return NULL;
  }

  Tokeniser *tokeniser = tokeniser_create();
  if(!tokeniser) {
    PERROR("Failed to allocate tokeniser.\n");
    return NULL;
  }

  char *read = src;

  while(*read) {
    // Skip whitespace
    while(*read && isspace((unsigned char)*read)) read++;
    if(!*read) break;

    // Try every possible token type, and select the longest one
    Token *tok = NULL;
    Token *candidates[3];
    candidates[0] = tokenise_keyword(read);
    candidates[1] = tokenise_ident(read);
    candidates[2] = tokenise_int_lit(read);

    size_t max_len = 0;
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
      if(!candidates[i]) continue;
      if(strlen(candidates[i]->value) > max_len) {
        max_len = strlen(candidates[i]->value);
        tok = candidates[i];
      }
    }

    // Destroy every other token
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
      if(!candidates[i]) continue;
      if(candidates[i] != tok) token_destroy(&candidates[i]);
    }

    if(tok) {
      // Add to tokens
      tokeniser_append(tokeniser, tok);
      // Skip to after the token
      read += strlen(tok->value);
    } else {
      // Invalid token
      goto err;
    }
  }

  // Handle tokeniser failure
  if(tokeniser->status != 0) goto err;

  return tokeniser;

err:
  PERROR("Failed to tokenise.\n");
  tokeniser_destroy(&tokeniser);
  return NULL;
}

// Helper function to convert token type to string
static const char *token_type_to_str(TokenType t) {
  static const char *token_type_strings[] = {"TokenInteger",  "TokenReal",        "TokenBoolean",       "TokenCharacter", "TokenSet",        "TokenTo",         "TokenAdd",
                                             "TokenSubtract", "TokenDivide",      "TokenMultiply",      "TokenExponent",  "TokenModulo",     "TokenIntDiv",     "TokenEqualTo",
                                             "TokenNEqualTo", "TokenGreaterThan", "TokenGreaterThanEq", "TokenLessThan",  "TokenLessThanEq", "TokenIdentifier", "TokenIntLit"};
  if(t >= 0 && t < sizeof(token_type_strings) / sizeof(token_type_strings[0]))
    return token_type_strings[t];
  else
    return "UNKNOWN";
}

// Print a token
static void token_print(Token *token) {
  if(!token) {
    printf("(null)");
    return;
  }
  printf("(Token) {.type = %s, .value = \"%s\"}", token_type_to_str(token->type), token->value);
}

// Print the full state of a tokeniser
void tokeniser_dump(Tokeniser *tokeniser) {
  if(!tokeniser) {
    printf("(null)\n");
    return;
  }
  printf("(Tokeniser) {\n");
  printf("  Token **tokens = {\n");
  for(size_t i = 0; i < tokeniser->count; i++) {
    printf("    ");
    token_print(tokeniser->tokens[i]);
    putchar('\n');
  }
  printf("  }\n");
  printf("  size_t count = %zu\n", tokeniser->count);
  printf("  size_t alloced = %zu\n", tokeniser->alloced);
  printf("  int status = %d\n", tokeniser->status);
  printf("  size_t read = %zu\n", tokeniser->read);
  printf("}\n");
}
