#ifndef TOKENISER_H

#define TOKENISER_H

#include <stddef.h>

typedef enum {
  // Datatype keywords
  TokenInteger, // INTEGER
  TokenReal, // REAL
  TokenBoolean, // BOOLEAN
  TokenCharacter, // CHARACTER
  // Action ? keywods
  TokenSet, // SET
  TokenTo, // TO
  // Operators
  // Arithmetic
  TokenAdd, // +
  TokenSubtract, // -
  TokenDivide, // /
  TokenMultiply, // *
  TokenExponent, // ^
  TokenModulo, // MOD
  TokenIntDiv, // DIV
  // Relational
  TokenEqualTo, // =
  TokenNEqualTo, // <> lmao wtf is this
  TokenGreaterThan, // >
  TokenGreaterThanEq, // >=
  TokenLessThan, // <
  TokenLessThanEq, // <=
  // Other things
  TokenIdentifier, // MyValue, myValue, My_Value, Counter2
  TokenIntLit // 1, -1, 1234 
} TokenType;

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
