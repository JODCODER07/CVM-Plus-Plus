#ifndef cvm_lexer_h
#define cvm_lexer_h

#include "common.h"
#include <string>

enum TokenType {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR, TOKEN_COLON,
    TOKEN_QUESTION, TOKEN_DOT_DOT_DOT,
    
    // One or two character tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    
    // Keywords.
    TOKEN_AND, TOKEN_BREAK, TOKEN_CLASS, TOKEN_CONTINUE, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_IMPORT, TOKEN_IN, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE, TOKEN_TRY, TOKEN_CATCH, TOKEN_THROW,

    TOKEN_ERROR, TOKEN_EOF
};

struct Token {
    TokenType type;
    const char* start;
    int length;
    int line;
};

class Lexer {
public:
    Lexer(const char* source);
    Token scanToken();

private:
    void skipWhitespace();
    Token makeToken(TokenType type);
    Token errorToken(const char* message);
    
    char advance();
    char peek();
    char peekNext();
    bool match(char expected);
    
    bool isAtEnd();
    bool isDigit(char c);
    bool isAlpha(char c);
    
    Token string();
    Token number();
    Token identifier();
    TokenType identifierType();
    TokenType checkKeyword(int startOffset, int length, const char* rest, TokenType type);

    const char* start;
    const char* current;
    int line;
};

#endif
