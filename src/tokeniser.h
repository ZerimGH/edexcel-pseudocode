#ifndef TOKENISER_H

#define TOKENISER_H

#include <stddef.h>

typedef enum {
  // Datatype keywords
  TokenInteger,   // INTEGER
  TokenReal,      // REAL
  TokenBoolean,   // BOOLEAN
  TokenCharacter, // CHARACTER
  TokenArray,     // ARRAY
  TokenString,    // STRING
  // Attribute keywords
  TokenConst, // CONST
  // Action ? keywords
  TokenSet,       // SET
  TokenTo,        // TO
  TokenIf,        // IF
  TokenThen,      // THEN
  TokenElse,      // ELSE
  TokenEnd,       // END
  TokenWhile,     // WHILE
  TokenDo,        // DO
  TokenRepeat,    // REPEAT
  TokenUntil,     // UNTIL
  TokenTimes,     // TIMES
  TokenReceive,   // RECEIVE
  TokenSend,      // SEND
  TokenFrom,      // FROM
  TokenRead,      // READ
  TokenWrite,     // WRITE
  TokenProcedure, // PROCEDURE
  TokenFunction,  // FUNCTION
  TokenReturn,    // RETURN
  // Operators
  // Arithmetic
  TokenAdd,      // +
  TokenSubtract, // -
  TokenDivide,   // /
  TokenMultiply, // *
  TokenExponent, // ^
  TokenModulo,   // MOD
  TokenIntDiv,   // DIV
  // Relational
  TokenEqualTo,       // =
  TokenNEqualTo,      // <> lmao wtf is this
  TokenGreaterThan,   // >
  TokenGreaterThanEq, // >=
  TokenLessThan,      // <
  TokenLessThanEq,    // <=
  // Logical
  TokenAnd, // AND
  TokenOr,  // OR
  TokenNot, // NOT
  // Array
  TokenAppend, // &
  // Other things
  TokenIdentifier,   // MyValue, myValue, My_Value, Counter2
  TokenIntLit,       // 1, -1, 1234
  TokenRealLit,      // 1.0, 23.5, -0.007
  TokenBooleanLit,   // TRUE, FALSE
  TokenCharacterLit, // 'a', 'b', '0', '\n'
  TokenStringLit,    // "hello!", "AKPDAOPS"
} TokenType;

typedef struct {
  TokenType type;
  char *value;
  size_t line_no, char_no;
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
