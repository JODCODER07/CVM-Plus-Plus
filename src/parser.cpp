#include "parser.h"
#include <iostream>
#include <string>

Parser::Parser(Lexer& lexer) : lexer(lexer), hadError(false), panicMode(false) {
    advance(); // Load the first token
}

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TOKEN_EOF)) {
        statements.push_back(declaration());
    }
    return statements;
}

void Parser::advance() {
    previous = current;
    for (;;) {
        current = lexer.scanToken();
        if (current.type != TOKEN_ERROR) break;
        errorAtCurrent(current.start);
    }
}

bool Parser::check(TokenType type) {
    return current.type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

void Parser::consume(TokenType type, const char* message) {
    if (check(type)) {
        advance();
        return;
    }
    errorAtCurrent(message);
    throw ParseError(message);
}

std::unique_ptr<Stmt> Parser::declaration() {
    try {
        if (match(TOKEN_CLASS)) return classDeclaration();
        if (match(TOKEN_FUN)) return function("function");
        if (match(TOKEN_VAR)) return varDeclaration();
        return statement();
    } catch (ParseError& error) {
        synchronize();
        return nullptr;
    }
}

std::unique_ptr<Stmt> Parser::classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token name = previous;

    std::unique_ptr<VariableExpr> superclass = nullptr;
    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        superclass = std::make_unique<VariableExpr>(previous);
    }

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    std::vector<std::unique_ptr<FunctionStmt>> methods;
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        std::unique_ptr<Stmt> methodStmt = function("method");
        methods.push_back(std::unique_ptr<FunctionStmt>(static_cast<FunctionStmt*>(methodStmt.release())));
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    return std::make_unique<ClassStmt>(name, std::move(superclass), std::move(methods));
}

std::unique_ptr<Stmt> Parser::function(const char* kind) {
    consume(TOKEN_IDENTIFIER, (std::string("Expect ") + kind + " name.").c_str());
    Token name = previous;
    
    consume(TOKEN_LEFT_PAREN, (std::string("Expect '(' after ") + kind + " name.").c_str());
    std::vector<Token> parameters;
    bool isVariadic = false;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            if (match(TOKEN_DOT_DOT_DOT)) {
                isVariadic = true;
                consume(TOKEN_IDENTIFIER, "Expect parameter name after '...'.");
                parameters.push_back(previous);
                break; // variadic must be last
            }
            consume(TOKEN_IDENTIFIER, "Expect parameter name.");
            parameters.push_back(previous);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    
    consume(TOKEN_LEFT_BRACE, (std::string("Expect '{' before ") + kind + " body.").c_str());
    std::vector<std::unique_ptr<Stmt>> body = block();
    return std::make_unique<FunctionStmt>(name, std::move(parameters), std::move(body), isVariadic);
}

std::unique_ptr<Stmt> Parser::varDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    Token name = previous;
    
    std::unique_ptr<Expr> initializer = nullptr;
    if (match(TOKEN_EQUAL)) {
        initializer = expression();
    }
    
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    return std::make_unique<VarStmt>(name, std::move(initializer));
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match(TOKEN_BREAK))    return breakStatement();
    if (match(TOKEN_CONTINUE)) return continueStatement();
    if (match(TOKEN_FOR))   return forEachStatement();
    if (match(TOKEN_IF))    return ifStatement();
    if (match(TOKEN_IMPORT)) return importStatement();
    if (match(TOKEN_PRINT)) return printStatement();
    if (match(TOKEN_RETURN)) return returnStatement();
    if (match(TOKEN_TRY))   return tryStatement();
    if (match(TOKEN_THROW)) return throwStatement();
    if (match(TOKEN_WHILE)) return whileStatement();
    if (match(TOKEN_LEFT_BRACE)) return std::make_unique<BlockStmt>(block());
    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::breakStatement() {
    Token kw = previous;
    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");
    return std::make_unique<BreakStmt>(kw);
}

std::unique_ptr<Stmt> Parser::continueStatement() {
    Token kw = previous;
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
    return std::make_unique<ContinueStmt>(kw);
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    Token keyword = previous;
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TOKEN_SEMICOLON)) {
        value = expression();
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    return std::make_unique<ReturnStmt>(keyword, std::move(value));
}

std::unique_ptr<Stmt> Parser::tryStatement() {
    consume(TOKEN_LEFT_BRACE, "Expect '{' after 'try'.");
    std::vector<std::unique_ptr<Stmt>> tryBlock = block();
    
    consume(TOKEN_CATCH, "Expect 'catch' after try block.");
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'catch'.");
    consume(TOKEN_IDENTIFIER, "Expect exception variable name.");
    Token exceptionVar = previous;
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after exception variable.");
    
    consume(TOKEN_LEFT_BRACE, "Expect '{' before catch block.");
    std::vector<std::unique_ptr<Stmt>> catchBlock = block();
    
    return std::make_unique<TryStmt>(std::move(tryBlock), exceptionVar, std::move(catchBlock));
}

std::unique_ptr<Stmt> Parser::throwStatement() {
    std::unique_ptr<Expr> expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after thrown expression.");
    return std::make_unique<ThrowStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::importStatement() {
    Token keyword = previous;
    consume(TOKEN_STRING, "Expect a string path after 'import'.");
    std::string raw(previous.start, previous.length);
    std::string path = raw.substr(1, raw.size() - 2);
    consume(TOKEN_SEMICOLON, "Expect ';' after import path.");
    return std::make_unique<ImportStmt>(keyword, path);
}

std::unique_ptr<Stmt> Parser::forEachStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    consume(TOKEN_IDENTIFIER, "Expect loop variable name after '('.");
    Token name = previous;
    consume(TOKEN_IN, "Expect 'in' after loop variable name.");
    auto iterable = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after iterable expression.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before for-each body.");
    auto body = block();
    return std::make_unique<ForEachStmt>(name, std::move(iterable), std::move(body));
}

std::unique_ptr<Stmt> Parser::ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    std::unique_ptr<Expr> condition = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after if condition.");
    
    std::unique_ptr<Stmt> thenBranch = statement();
    std::unique_ptr<Stmt> elseBranch = nullptr;
    if (match(TOKEN_ELSE)) {
        elseBranch = statement();
    }
    
    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::whileStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    std::unique_ptr<Expr> condition = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    std::unique_ptr<Stmt> body = statement();
    
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
}

std::vector<std::unique_ptr<Stmt>> Parser::block() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        statements.push_back(declaration());
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return statements;
}

std::unique_ptr<Stmt> Parser::printStatement() {
    std::unique_ptr<Expr> value = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    return std::make_unique<PrintStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    std::unique_ptr<Expr> expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

std::unique_ptr<Expr> Parser::expression() {
    return assignment();
}

std::unique_ptr<Expr> Parser::assignment() {
    std::unique_ptr<Expr> expr = ternary();

    if (match(TOKEN_EQUAL)) {
        Token equals = previous;
        std::unique_ptr<Expr> value = assignment();

        if (VariableExpr* v = dynamic_cast<VariableExpr*>(expr.get())) {
            Token name = v->name;
            return std::make_unique<AssignExpr>(name, std::move(value));
        } else if (IndexExpr* idx = dynamic_cast<IndexExpr*>(expr.get())) {
            return std::make_unique<IndexAssignExpr>(
                std::move(idx->object), idx->bracket, std::move(idx->index), std::move(value));
        } else if (GetExpr* get = dynamic_cast<GetExpr*>(expr.get())) {
            return std::make_unique<SetExpr>(std::move(get->object), get->name, std::move(value));
        }

        errorAt(&equals, "Invalid assignment target.");
    }

    return expr;
}

std::unique_ptr<Expr> Parser::ternary() {
    auto cond = logic_or();
    if (match(TOKEN_QUESTION)) {
        auto thenE = expression();
        consume(TOKEN_COLON, "Expect ':' after ternary 'then' branch.");
        auto elseE = ternary(); // right-associative
        return std::make_unique<TernaryExpr>(std::move(cond), std::move(thenE), std::move(elseE));
    }
    return cond;
}

std::unique_ptr<Expr> Parser::logic_or() {
    std::unique_ptr<Expr> expr = logic_and();
    while (match(TOKEN_OR)) {
        Token op = previous;
        std::unique_ptr<Expr> right = logic_and();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::logic_and() {
    std::unique_ptr<Expr> expr = equality();
    while (match(TOKEN_AND)) {
        Token op = previous;
        std::unique_ptr<Expr> right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    std::unique_ptr<Expr> expr = comparison();
    while (match(TOKEN_BANG_EQUAL) || match(TOKEN_EQUAL_EQUAL)) {
        Token op = previous;
        std::unique_ptr<Expr> right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    std::unique_ptr<Expr> expr = term();
    while (match(TOKEN_GREATER) || match(TOKEN_GREATER_EQUAL) || match(TOKEN_LESS) || match(TOKEN_LESS_EQUAL)) {
        Token op = previous;
        std::unique_ptr<Expr> right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    std::unique_ptr<Expr> expr = factor();
    while (match(TOKEN_MINUS) || match(TOKEN_PLUS)) {
        Token op = previous;
        std::unique_ptr<Expr> right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    std::unique_ptr<Expr> expr = unary();
    while (match(TOKEN_SLASH) || match(TOKEN_STAR)) {
        Token op = previous;
        std::unique_ptr<Expr> right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match(TOKEN_BANG) || match(TOKEN_MINUS)) {
        Token op = previous;
        std::unique_ptr<Expr> right = unary();
        return std::make_unique<UnaryExpr>(op, std::move(right));
    }
    return call();
}

std::unique_ptr<Expr> Parser::call() {
    std::unique_ptr<Expr> expr = primary();
    
    while (true) {
        if (match(TOKEN_LEFT_PAREN)) {
            expr = finishCall(std::move(expr));
        } else if (match(TOKEN_LEFT_BRACKET)) {
            expr = finishIndex(std::move(expr));
        } else if (match(TOKEN_DOT)) {
            consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
            Token name = previous;
            expr = std::make_unique<GetExpr>(std::move(expr), name);
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::finishCall(std::unique_ptr<Expr> callee) {
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            if (arguments.size() >= 255) {
                errorAtCurrent("Can't have more than 255 arguments.");
            }
            arguments.push_back(expression());
        } while (match(TOKEN_COMMA));
    }
    Token paren = current;
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return std::make_unique<CallExpr>(std::move(callee), paren, std::move(arguments));
}

std::unique_ptr<Expr> Parser::finishIndex(std::unique_ptr<Expr> object) {
    Token bracket = previous;
    std::unique_ptr<Expr> index = expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
    return std::make_unique<IndexExpr>(std::move(object), bracket, std::move(index));
}

std::unique_ptr<Expr> Parser::primary() {
    if (match(TOKEN_FALSE)) return std::make_unique<LiteralExpr>(Value(false));
    if (match(TOKEN_TRUE)) return std::make_unique<LiteralExpr>(Value(true));
    if (match(TOKEN_THIS)) return std::make_unique<ThisExpr>(previous);
    if (match(TOKEN_SUPER)) {
        Token keyword = previous;
        consume(TOKEN_DOT, "Expect '.' after 'super'.");
        consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
        Token method = previous;
        return std::make_unique<SuperExpr>(keyword, method);
    }
    
    if (match(TOKEN_STRING)) {
        std::string raw(previous.start + 1, previous.length - 2);
        // Detect interpolated string: "...${expr}..."
        if (raw.find("${") != std::string::npos) {
            return buildInterpolation(raw, previous.line);
        }
        return std::make_unique<LiteralExpr>(Value(raw));
    }
    
    if (match(TOKEN_NUMBER)) {
        std::string str(previous.start, previous.length);
        double val = std::stod(str);
        return std::make_unique<LiteralExpr>(Value(val));
    }
    
    if (match(TOKEN_IDENTIFIER)) {
        return std::make_unique<VariableExpr>(previous);
    }
    
    if (match(TOKEN_LEFT_BRACKET)) {
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TOKEN_RIGHT_BRACKET)) {
            do {
                if (elements.size() >= 255) {
                    errorAtCurrent("Cannot have more than 255 elements.");
                }
                elements.push_back(expression());
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");
        return std::make_unique<ArrayExpr>(std::move(elements));
    }

    if (match(TOKEN_LEFT_BRACE)) {
        std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> entries;
        if (!check(TOKEN_RIGHT_BRACE)) {
            do {
                if (entries.size() >= 255) {
                    errorAtCurrent("Cannot have more than 255 entries.");
                }
                std::unique_ptr<Expr> key = expression();
                consume(TOKEN_COLON, "Expect ':' after map key.");
                std::unique_ptr<Expr> value = expression();
                entries.push_back(std::make_pair(std::move(key), std::move(value)));
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RIGHT_BRACE, "Expect '}' after map entries.");
        return std::make_unique<MapExpr>(std::move(entries));
    }

    if (match(TOKEN_LEFT_PAREN)) {
        std::unique_ptr<Expr> expr = expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        return expr;
    }
    
    errorAtCurrent("Expect expression.");
    throw ParseError("Expect expression.");
}

void Parser::errorAtCurrent(const char* message) {
    errorAt(&current, message);
}

void Parser::error(const char* message) {
    errorAt(&previous, message);
}

void Parser::errorAt(Token* token, const char* message) {
    if (panicMode) return;
    panicMode = true;
    
    std::cerr << "[line " << token->line << "] Error";
    
    if (token->type == TOKEN_EOF) {
        std::cerr << " at end";
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        std::string lexeme(token->start, token->length);
        std::cerr << " at '" << lexeme << "'";
    }
    
    std::cerr << ": " << message << std::endl;
    hadError = true;
}

void Parser::synchronize() {
    panicMode = false;
    
    while (current.type != TOKEN_EOF) {
        if (previous.type == TOKEN_SEMICOLON) return;
        
        switch (current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ;
        }
        advance();
    }
}

// ─── String interpolation helpers ────────────────────────────────────────────

std::unique_ptr<Expr> Parser::parseOneExpr() {
    return expression();
}

std::unique_ptr<Expr> Parser::buildInterpolation(const std::string& raw, int line) {
    std::unique_ptr<Expr> result = nullptr;
    size_t pos = 0;

    auto makePlusTok = [&]() {
        Token t; t.type = TOKEN_PLUS; t.start = "+"; t.length = 1; t.line = line;
        return t;
    };
    auto concatExprs = [&](std::unique_ptr<Expr> left, std::unique_ptr<Expr> right) -> std::unique_ptr<Expr> {
        return std::make_unique<BinaryExpr>(std::move(left), makePlusTok(), std::move(right));
    };

    while (true) {
        size_t dollarPos = raw.find("${", pos);

        // Literal part before next ${ (or to end)
        std::string literal = (dollarPos == std::string::npos)
            ? raw.substr(pos)
            : raw.substr(pos, dollarPos - pos);
        std::unique_ptr<Expr> litExpr = std::make_unique<LiteralExpr>(Value(literal));
        if (result) {
            result = concatExprs(std::move(result), std::move(litExpr));
        } else {
            result = std::move(litExpr);
        }

        if (dollarPos == std::string::npos) break;

        // Find closing }
        size_t closePos = raw.find("}", dollarPos + 2);
        if (closePos == std::string::npos) break;

        std::string exprSrc = raw.substr(dollarPos + 2, closePos - dollarPos - 2);

        // Keep source string alive so Lexer's const char* stays valid
        interpSources.push_back(exprSrc);
        Lexer subLex(interpSources.back().c_str());
        Parser subParser(subLex);
        auto exprNode = subParser.parseOneExpr();

        // Wrap in str() call to stringify the expression value
        Token strTok;
        strTok.type = TOKEN_IDENTIFIER;
        static const char* strName = "str";
        strTok.start = strName; strTok.length = 3; strTok.line = line;
        auto callee = std::make_unique<VariableExpr>(strTok);
        std::vector<std::unique_ptr<Expr>> callArgs;
        callArgs.push_back(std::move(exprNode));
        Token parenTok;
        parenTok.type = TOKEN_LEFT_PAREN; parenTok.start = "("; parenTok.length = 1; parenTok.line = line;
        std::unique_ptr<Expr> strCall = std::make_unique<CallExpr>(std::move(callee), parenTok, std::move(callArgs));

        if (result) {
            result = concatExprs(std::move(result), std::move(strCall));
        } else {
            result = std::move(strCall);
        }
        pos = closePos + 1;
    }

    if (result) {
        return result;
    } else {
        return std::make_unique<LiteralExpr>(Value(std::string("")));
    }
}
