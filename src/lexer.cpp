#include "lexer.h"
#include <cstring>

Lexer::Lexer(const char* source) {
    start = source;
    current = source;
    line = 1;
}

bool Lexer::isAtEnd() {
    return *current == '\0';
}

char Lexer::advance() {
    current++;
    return current[-1];
}

char Lexer::peek() {
    return *current;
}

char Lexer::peekNext() {
    if (isAtEnd()) return '\0';
    return current[1];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (*current != expected) return false;
    current++;
    return true;
}

Token Lexer::makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = start;
    token.length = (int)(current - start);
    token.line = line;
    return token;
}

Token Lexer::errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = line;
    return token;
}

void Lexer::skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

Token Lexer::string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_STRING);
}

Token Lexer::number() {
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        advance(); // Consume the "."
        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

TokenType Lexer::checkKeyword(int startOffset, int length, const char* rest, TokenType type) {
    if (current - start == startOffset + length &&
        memcmp(start + startOffset, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

TokenType Lexer::identifierType() {
    switch (start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (current - start > 1) {
                switch (start[1]) {
                    case 'a': return checkKeyword(2, 3, "tch", TOKEN_CATCH);
                    case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
            break;
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (current - start > 1) {
                switch (start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            if (current - start > 1) {
                switch (start[1]) {
                    case 'f': return checkKeyword(2, 0, "", TOKEN_IF);
                    case 'm': return checkKeyword(2, 4, "port", TOKEN_IMPORT);
                    case 'n': return checkKeyword(2, 0, "", TOKEN_IN);
                }
            }
            break;
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (current - start > 1) {
                switch (start[1]) {
                    case 'h': {
                        if (current - start > 2) {
                            switch (start[2]) {
                                case 'i': return checkKeyword(3, 1, "s", TOKEN_THIS);
                                case 'r': return checkKeyword(3, 2, "ow", TOKEN_THROW);
                            }
                        }
                        break;
                    }
                    case 'r': {
                        if (current - start == 3) {
                            return checkKeyword(2, 1, "y", TOKEN_TRY);
                        } else {
                            return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                        }
                    }
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

Token Lexer::identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

Token Lexer::scanToken() {
    skipWhitespace();
    start = current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_BRACKET);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.':
            if (peek() == '.' && peekNext() == '.') { advance(); advance(); return makeToken(TOKEN_DOT_DOT_DOT); }
            return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case ':': return makeToken(TOKEN_COLON);
        case '?': return makeToken(TOKEN_QUESTION);
        
        case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        
        case '"': return string();
    }

    return errorToken("Unexpected character.");
}
