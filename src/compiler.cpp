#include "compiler.h"
#include "vm.h"
#include <iostream>

Compiler::Compiler(VM& vm) : vm(vm) {
    scopeDepth = 0;
    currentLine = 1;
    currentChunk = nullptr;
    pushState("<script>", 0);
}

void Compiler::pushState(std::string name, int arity, bool isMethod) {
    if (currentFunction) {
        states.push_back({currentFunction, currentChunk, locals, upvalues, scopeDepth});
    }
    currentFunction = vm.allocateObject<ObjFunction>();
    currentFunction->name = name;
    currentFunction->arity = arity;
    currentChunk = currentFunction->chunk;
    locals.clear();
    upvalues.clear();
    scopeDepth = 0;
    
    // Claim slot 0 for the function/script itself
    Local local;
    if (isMethod) {
        local.name.start = "this";
        local.name.length = 4;
    } else {
        local.name.start = "";
        local.name.length = 0;
    }
    local.depth = 0;
    local.isCaptured = false;
    locals.push_back(local);
}

ObjFunction* Compiler::popState() {
    ObjFunction* fn = currentFunction;
    if (!states.empty()) {
        CompilerState prevState = states.back();
        states.pop_back();
        currentFunction = prevState.function;
        currentChunk = prevState.chunk;
        locals = prevState.locals;
        upvalues = prevState.upvalues;
        scopeDepth = prevState.scopeDepth;
    } else {
        currentFunction = nullptr;
        currentChunk = nullptr;
    }
    return fn;
}

void Compiler::beginScope() {
    scopeDepth++;
}

void Compiler::endScope() {
    scopeDepth--;
    while (!locals.empty() && locals.back().depth > scopeDepth) {
        if (locals.back().isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        locals.pop_back();
    }
}

void Compiler::addLocal(Token name) {
    if (locals.size() >= 256) {
        // Error: too many locals
        return;
    }
    Local local;
    local.name = name;
    local.depth = scopeDepth;
    local.isCaptured = false;
    locals.push_back(local);
}

int Compiler::resolveLocal(Token name) {
    for (int i = locals.size() - 1; i >= 0; i--) {
        Local* local = &locals[i];
        std::string localName;
        if (local->name.start && local->name.length > 0) {
            localName = std::string(local->name.start, local->name.length);
        }
        std::string resolveName;
        if (name.start && name.length > 0) {
            resolveName = std::string(name.start, name.length);
        }
        if (localName == resolveName && !localName.empty()) {
            return i;
        }
    }
    return -1;
}

int Compiler::resolveLocalInState(const std::vector<Local>& stateLocals, Token name) {
    for (int i = stateLocals.size() - 1; i >= 0; i--) {
        const Local& local = stateLocals[i];
        std::string localName;
        if (local.name.start && local.name.length > 0) {
            localName = std::string(local.name.start, local.name.length);
        }
        std::string resolveName;
        if (name.start && name.length > 0) {
            resolveName = std::string(name.start, name.length);
        }
        if (localName == resolveName && !localName.empty()) {
            return i;
        }
    }
    return -1;
}

int Compiler::resolveUpvalue(int stateIndex, Token name) {
    if (stateIndex == 0) return -1; // Root state (<script>) has no outer scope

    const std::vector<Local>& outerLocals = (stateIndex - 1 == states.size()) ? locals : states[stateIndex - 1].locals;
    int local = resolveLocalInState(outerLocals, name);
    if (local != -1) {
        if (stateIndex - 1 == states.size()) {
            locals[local].isCaptured = true;
        } else {
            states[stateIndex - 1].locals[local].isCaptured = true;
        }
        return addUpvalue(stateIndex, local, true);
    }

    int upvalue = resolveUpvalue(stateIndex - 1, name);
    if (upvalue != -1) {
        return addUpvalue(stateIndex, upvalue, false);
    }

    return -1;
}

int Compiler::addUpvalue(int stateIndex, int index, bool isLocal) {
    std::vector<Upvalue>& stateUpvalues = (stateIndex == states.size()) ? upvalues : states[stateIndex].upvalues;
    for (int i = 0; i < (int)stateUpvalues.size(); i++) {
        if (stateUpvalues[i].index == index && stateUpvalues[i].isLocal == isLocal) {
            return i;
        }
    }
    Upvalue up;
    up.index = index;
    up.isLocal = isLocal;
    stateUpvalues.push_back(up);
    return stateUpvalues.size() - 1;
}

void Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& statements, Chunk& chunk) {
    // Actually we don't use the provided chunk directly anymore for script body
    // we use the script function's chunk.
    for (const auto& stmt : statements) {
        if (stmt) stmt->accept(*this);
    }
    emitConstant(Value(0.0));
    emitByte(OP_RETURN);
    
    ObjFunction* scriptFn = popState();
    // Copy the script chunk into the provided chunk so the VM can run it
    chunk = *(scriptFn->chunk);
}

void Compiler::emitByte(uint8_t byte) {
    currentChunk->writeChunk(byte, currentLine);
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void Compiler::emitConstant(Value value) {
    if (value.isBool()) {
        emitByte(value.asBool() ? OP_TRUE : OP_FALSE);
    } else {
        emitBytes(OP_CONSTANT, currentChunk->addConstant(value));
    }
}

int Compiler::emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk->code.size() - 2;
}

void Compiler::patchJump(int offset) {
    int jump = currentChunk->code.size() - offset - 2;
    currentChunk->code[offset] = (jump >> 8) & 0xff;
    currentChunk->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int loopStart) {
    emitByte(OP_LOOP);
    int jump = currentChunk->code.size() - loopStart + 2;
    emitByte((jump >> 8) & 0xff);
    emitByte(jump & 0xff);
}

void Compiler::visitBinaryExpr(BinaryExpr* expr) {
    currentLine = expr->op.line;
    expr->left->accept(*this);
    expr->right->accept(*this);
    switch (expr->op.type) {
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
        default: break;
    }
}

void Compiler::visitLogicalExpr(LogicalExpr* expr) {
    currentLine = expr->op.line;
    if (expr->op.type == TOKEN_OR) {
        expr->left->accept(*this);
        int elseJump = emitJump(OP_JUMP_IF_FALSE);
        int endJump = emitJump(OP_JUMP);
        patchJump(elseJump);
        emitByte(OP_POP);
        expr->right->accept(*this);
        patchJump(endJump);
    } else { // TOKEN_AND
        expr->left->accept(*this);
        int endJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
        expr->right->accept(*this);
        patchJump(endJump);
    }
}

void Compiler::visitLiteralExpr(LiteralExpr* expr) {
    emitConstant(expr->value);
}

void Compiler::visitVariableExpr(VariableExpr* expr) {
    currentLine = expr->name.line;
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        emitBytes(OP_GET_LOCAL, arg);
    } else if ((arg = resolveUpvalue(states.size(), expr->name)) != -1) {
        emitBytes(OP_GET_UPVALUE, arg);
    } else {
        std::string name(expr->name.start, expr->name.length);
        int index = currentChunk->addString(name);
        emitBytes(OP_GET_GLOBAL, index);
    }
}

void Compiler::visitAssignExpr(AssignExpr* expr) {
    currentLine = expr->name.line;
    expr->value->accept(*this);
    
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        emitBytes(OP_SET_LOCAL, arg);
    } else if ((arg = resolveUpvalue(states.size(), expr->name)) != -1) {
        emitBytes(OP_SET_UPVALUE, arg);
    } else {
        std::string name(expr->name.start, expr->name.length);
        int index = currentChunk->addString(name);
        emitBytes(OP_SET_GLOBAL, index);
    }
}

void Compiler::visitUnaryExpr(UnaryExpr* expr) {
    currentLine = expr->op.line;
    expr->right->accept(*this);
    switch (expr->op.type) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        default: break;
    }
}

void Compiler::visitCallExpr(CallExpr* expr) {
    expr->callee->accept(*this);
    for (const auto& arg : expr->arguments) {
        arg->accept(*this);
    }
    emitBytes(OP_CALL, expr->arguments.size());
}

void Compiler::visitArrayExpr(ArrayExpr* expr) {
    for (const auto& element : expr->elements) {
        element->accept(*this);
    }
    emitBytes(OP_BUILD_ARRAY, expr->elements.size());
}

void Compiler::visitMapExpr(MapExpr* expr) {
    for (const auto& entry : expr->entries) {
        entry.first->accept(*this);
        entry.second->accept(*this);
    }
    emitBytes(OP_BUILD_MAP, expr->entries.size());
}

void Compiler::visitIndexExpr(IndexExpr* expr) {
    expr->object->accept(*this);
    expr->index->accept(*this);
    emitByte(OP_INDEX_GET);
}

void Compiler::visitIndexAssignExpr(IndexAssignExpr* expr) {
    expr->object->accept(*this);
    expr->index->accept(*this);
    expr->value->accept(*this);
    emitByte(OP_INDEX_SET);
}

void Compiler::visitExpressionStmt(ExpressionStmt* stmt) {
    stmt->expression->accept(*this);
    emitByte(OP_POP);
}

void Compiler::visitPrintStmt(PrintStmt* stmt) {
    stmt->expression->accept(*this);
    emitByte(OP_PRINT);
}

void Compiler::visitVarStmt(VarStmt* stmt) {
    currentLine = stmt->name.line;
    if (stmt->initializer) {
        stmt->initializer->accept(*this);
    } else {
        emitConstant(Value(0.0));
    }
    
    if (scopeDepth > 0) {
        addLocal(stmt->name);
        return; // Locals don't need OP_DEFINE_GLOBAL, they just stay on the stack!
    }
    
    std::string name(stmt->name.start, stmt->name.length);
    int index = currentChunk->addString(name);
    emitBytes(OP_DEFINE_GLOBAL, index);
}

void Compiler::visitBlockStmt(BlockStmt* stmt) {
    beginScope();
    for (const auto& statement : stmt->statements) {
        if (statement) statement->accept(*this);
    }
    endScope();
}

void Compiler::visitIfStmt(IfStmt* stmt) {
    stmt->condition->accept(*this);
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    
    stmt->thenBranch->accept(*this);
    
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);
    
    if (stmt->elseBranch) {
        stmt->elseBranch->accept(*this);
    }
    patchJump(elseJump);
}

void Compiler::visitWhileStmt(WhileStmt* stmt) {
    int loopStart = currentChunk->code.size();

    // Push loop context for break/continue
    loopStack.push_back({loopStart, {}, (int)locals.size()});

    stmt->condition->accept(*this);
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    
    stmt->body->accept(*this);
    emitLoop(loopStart);
    
    patchJump(exitJump);
    emitByte(OP_POP);

    // Patch all break jumps to here
    LoopContext ctx = loopStack.back(); loopStack.pop_back();
    for (int bj : ctx.breakJumps) patchJump(bj);
}

void Compiler::visitFunctionStmt(FunctionStmt* stmt) {
    currentLine = stmt->name.line;
    std::string name(stmt->name.start, stmt->name.length);
    int arity = stmt->isVariadic ? (int)stmt->params.size() - 1 : (int)stmt->params.size();
    pushState(name, arity);
    currentFunction->isVariadic = stmt->isVariadic;
    
    beginScope();
    for (Token param : stmt->params) {
        addLocal(param);
    }
    
    for (const auto& statement : stmt->body) {
        if (statement) statement->accept(*this);
    }
    
    // Default return
    emitConstant(Value(0.0));
    emitByte(OP_RETURN);
    
    std::vector<Upvalue> compiledUpvalues = upvalues;
    ObjFunction* function = popState();
    function->upvalueCount = compiledUpvalues.size();
    
    int constantIndex = currentChunk->addConstant(Value(function));
    emitBytes(OP_CLOSURE, constantIndex);
    
    for (const auto& up : compiledUpvalues) {
        emitByte(up.isLocal ? 1 : 0);
        emitByte(up.index);
    }
    
    if (scopeDepth > 0) {
        addLocal(stmt->name);
    } else {
        int index = currentChunk->addString(name);
        emitBytes(OP_DEFINE_GLOBAL, index);
    }
}

void Compiler::visitReturnStmt(ReturnStmt* stmt) {
    currentLine = stmt->keyword.line;
    if (stmt->value) {
        stmt->value->accept(*this);
    } else {
        if (currentFunction && currentFunction->name == "init") {
            emitBytes(OP_GET_LOCAL, 0);
        } else {
            emitConstant(Value(0.0));
        }
    }
    emitByte(OP_RETURN);
}

void Compiler::visitClassStmt(ClassStmt* stmt) {
    currentLine = stmt->name.line;
    std::string name(stmt->name.start, stmt->name.length);
    int nameIndex = currentChunk->addString(name);
    
    emitBytes(OP_CLASS, nameIndex);
    
    if (scopeDepth > 0) {
        addLocal(stmt->name);
    } else {
        emitBytes(OP_DEFINE_GLOBAL, nameIndex);
    }
    
    if (stmt->superclass != nullptr) {
        std::string superName(stmt->superclass->name.start, stmt->superclass->name.length);
        if (name == superName) {
            std::cerr << "Error: A class can't inherit from itself." << std::endl;
        }
        
        // Start a scope to capture "super" upvalue
        beginScope();
        
        // Push the superclass onto the stack (this is the initial value of the "super" local variable)
        stmt->superclass->accept(*this);
        
        Token superToken = { TOKEN_SUPER, "super", 5, stmt->name.line };
        addLocal(superToken);
        
        // Push subclass onto the stack
        if (scopeDepth > 1) { // subclass is local in parent scope
            int slot = resolveLocal(stmt->name);
            emitBytes(OP_GET_LOCAL, slot);
        } else {
            emitBytes(OP_GET_GLOBAL, nameIndex);
        }
        
        // Stack now has: superclass ("super" local) at peek(1), subclass at peek(0)
        emitByte(OP_INHERIT); // copies methods and pops subclass
        
        // Push subclass onto the stack again so it sits above "super" during methods compilation
        if (scopeDepth > 1) {
            int slot = resolveLocal(stmt->name);
            emitBytes(OP_GET_LOCAL, slot);
        } else {
            emitBytes(OP_GET_GLOBAL, nameIndex);
        }
    } else {
        // No superclass: push subclass onto stack so it is at peek(0) during methods compilation
        if (scopeDepth > 0) {
            int slot = resolveLocal(stmt->name);
            emitBytes(OP_GET_LOCAL, slot);
        } else {
            emitBytes(OP_GET_GLOBAL, nameIndex);
        }
    }
    
    for (const auto& method : stmt->methods) {
        std::string methodName(method->name.start, method->name.length);
        int methodIndex = currentChunk->addString(methodName);
        
        pushState(methodName, method->params.size(), true);
        
        beginScope();
        for (Token param : method->params) {
            addLocal(param);
        }
        
        for (const auto& statement : method->body) {
            if (statement) statement->accept(*this);
        }
        
        if (methodName == "init") {
            emitBytes(OP_GET_LOCAL, 0);
        } else {
            emitConstant(Value(0.0));
        }
        emitByte(OP_RETURN);
        
        std::vector<Upvalue> compiledUpvalues = upvalues;
        ObjFunction* function = popState();
        function->upvalueCount = compiledUpvalues.size();
        
        int constantIndex = currentChunk->addConstant(Value(function));
        emitBytes(OP_CLOSURE, constantIndex);
        
        for (const auto& up : compiledUpvalues) {
            emitByte(up.isLocal ? 1 : 0);
            emitByte(up.index);
        }
        
        emitBytes(OP_METHOD, methodIndex);
    }
    
    // Clean up stack
    emitByte(OP_POP); // Pop subclass reference
    
    if (stmt->superclass != nullptr) {
        endScope(); // Close "super" scope, which pops "super" local variable
    }
}

void Compiler::visitGetExpr(GetExpr* expr) {
    expr->object->accept(*this);
    int nameIndex = currentChunk->addString(std::string(expr->name.start, expr->name.length));
    emitBytes(OP_GET_PROPERTY, nameIndex);
}

void Compiler::visitSetExpr(SetExpr* expr) {
    expr->object->accept(*this);
    expr->value->accept(*this);
    int nameIndex = currentChunk->addString(std::string(expr->name.start, expr->name.length));
    emitBytes(OP_SET_PROPERTY, nameIndex);
}

void Compiler::visitThisExpr(ThisExpr* expr) {
    int arg = resolveLocal(expr->keyword);
    if (arg != -1) {
        emitBytes(OP_GET_LOCAL, arg);
    } else if ((arg = resolveUpvalue(states.size(), expr->keyword)) != -1) {
        emitBytes(OP_GET_UPVALUE, arg);
    } else {
        std::cerr << "Error: 'this' can only be used inside a class method." << std::endl;
    }
}

void Compiler::visitSuperExpr(SuperExpr* expr) {
    Token superToken = { TOKEN_SUPER, "super", 5, expr->keyword.line };
    int superSlot = resolveUpvalue(states.size(), superToken);
    if (superSlot == -1) {
        std::cerr << "Error: Can't use 'super' outside of a subclass." << std::endl;
    }
    
    // We also need the receiver 'this' to bind the method.
    Token thisToken = { TOKEN_THIS, "this", 4, expr->keyword.line };
    int thisSlot = resolveLocal(thisToken);
    if (thisSlot != -1) {
        emitBytes(OP_GET_LOCAL, thisSlot);
    } else if ((thisSlot = resolveUpvalue(states.size(), thisToken)) != -1) {
        emitBytes(OP_GET_UPVALUE, thisSlot);
    } else {
        std::cerr << "Error: 'this' not found for 'super' call." << std::endl;
    }
    
    // Push 'super' (the superclass) onto stack
    emitBytes(OP_GET_UPVALUE, superSlot);
    
    // Look up method in superclass
    int nameIndex = currentChunk->addString(std::string(expr->method.start, expr->method.length));
    emitBytes(OP_GET_SUPER, nameIndex);
}

void Compiler::visitTryStmt(TryStmt* stmt) {
    int catchJump = emitJump(OP_TRY);
    
    for (const auto& statement : stmt->tryBlock) {
        statement->accept(*this);
    }
    
    emitByte(OP_END_TRY);
    int endJump = emitJump(OP_JUMP);
    
    patchJump(catchJump);
    
    beginScope();
    addLocal(stmt->exceptionVar);
    
    for (const auto& statement : stmt->catchBlock) {
        statement->accept(*this);
    }
    
    endScope();
    
    patchJump(endJump);
}

void Compiler::visitThrowStmt(ThrowStmt* stmt) {
    stmt->expression->accept(*this);
    emitByte(OP_THROW);
}

void Compiler::visitImportStmt(ImportStmt* stmt) {
    currentLine = stmt->keyword.line;
    int pathIndex = currentChunk->addString(stmt->path);
    emitBytes(OP_IMPORT, (uint8_t)pathIndex);
    emitByte(OP_POP); // discard module return value left on stack
}

void Compiler::visitForEachStmt(ForEachStmt* stmt) {
    currentLine = stmt->name.line;
    
    beginScope();
    
    // Compile iterable → $coll local (stays on stack as its slot)
    stmt->iterable->accept(*this);
    Token collTok;
    collTok.type = TOKEN_IDENTIFIER;
    static const char* collName = "$coll";
    collTok.start = collName;
    collTok.length = 5;
    collTok.line = stmt->name.line;
    addLocal(collTok);
    int collSlot = (int)locals.size() - 1;

    // Push 0 → $i local (iteration index)
    emitConstant(Value(0.0));
    Token iTok;
    iTok.type = TOKEN_IDENTIFIER;
    static const char* iName = "$i";
    iTok.start = iName;
    iTok.length = 2;
    iTok.line = stmt->name.line;
    addLocal(iTok);
    int idxSlot = (int)locals.size() - 1;

    // ── Loop start ──────────────────────────────────────────────
    int loopStart = (int)currentChunk->code.size();

    // Push loop context for break/continue
    loopStack.push_back({loopStart, {}, (int)locals.size()});

    // Condition: size > $i
    emitBytes(OP_COLLECTION_SIZE, (uint8_t)collSlot); // push size
    emitBytes(OP_GET_LOCAL, (uint8_t)idxSlot);        // push $i
    emitByte(OP_GREATER);                              // size > $i
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // pop true

    // Inner scope: bind loop variable = coll[$i]
    beginScope();
    emitByte(OP_ITER_GET);
    emitByte((uint8_t)collSlot);
    emitByte((uint8_t)idxSlot); // push element
    addLocal(stmt->name); // x = element on stack top

    for (const auto& s : stmt->body) {
        if (s) s->accept(*this);
    }

    endScope(); // pop x

    // Increment $i
    emitBytes(OP_GET_LOCAL, (uint8_t)idxSlot);
    emitConstant(Value(1.0));
    emitByte(OP_ADD);
    emitBytes(OP_SET_LOCAL, (uint8_t)idxSlot);
    emitByte(OP_POP);

    emitLoop(loopStart);
    // ── Loop end ────────────────────────────────────────────────

    patchJump(exitJump);
    emitByte(OP_POP); // pop false

    // Patch break jumps
    LoopContext ctx = loopStack.back(); loopStack.pop_back();
    for (int bj : ctx.breakJumps) patchJump(bj);

    endScope(); // pop $coll, $i
}

// ─── break / continue ────────────────────────────────────────────────────────

void Compiler::visitBreakStmt(BreakStmt* stmt) {
    currentLine = stmt->keyword.line;
    if (loopStack.empty()) { return; } // error: break outside loop
    // Pop any locals declared since loop entry
    int loopLocalDepth = loopStack.back().localDepthAtEntry;
    for (int i = (int)locals.size() - 1; i >= loopLocalDepth; i--) {
        emitByte(OP_POP);
    }
    int jump = emitJump(OP_JUMP);
    loopStack.back().breakJumps.push_back(jump);
}

void Compiler::visitContinueStmt(ContinueStmt* stmt) {
    currentLine = stmt->keyword.line;
    if (loopStack.empty()) { return; }
    int loopLocalDepth = loopStack.back().localDepthAtEntry;
    for (int i = (int)locals.size() - 1; i >= loopLocalDepth; i--) {
        emitByte(OP_POP);
    }
    emitLoop(loopStack.back().loopStart);
}

// ─── ternary expression ───────────────────────────────────────────────────────

void Compiler::visitTernaryExpr(TernaryExpr* expr) {
    expr->condition->accept(*this);
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // pop condition (true)
    expr->thenExpr->accept(*this);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP); // pop condition (false)
    expr->elseExpr->accept(*this);
    patchJump(endJump);
}
