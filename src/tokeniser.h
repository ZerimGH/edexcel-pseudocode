#ifndef TOKENISER_H

#define TOKENISER_H

#include <stddef.h>

typedef enum { TokenInteger, TokenReal, TokenBoolean, TokenCharacter, TokenSet, TokenTo, TokenIdentifier, TokenIntLit } TokenType;

typedef struct {
  TokenType type;
  char *value;
} Token;

typedef struct {
  Token **tokens;
  size_t count;
  size_t alloced;
  int status;
  size_t read;
} Tokeniser;

Tokeniser *tokenise(char *src);
void tokeniser_destroy(Tokeniser **tokeniser);
void tokeniser_dump(Tokeniser *tokeniser);
int tokeniser_done(Tokeniser *tokeniser);
Token *tokeniser_top(Tokeniser *tokeniser);
Token *tokeniser_expect(Tokeniser *tokeniser, size_t num, ...);

#endif
