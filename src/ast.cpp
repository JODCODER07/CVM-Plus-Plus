#include "ast.h"

void BinaryExpr::accept(Visitor& visitor) { visitor.visitBinaryExpr(this); }
void LogicalExpr::accept(Visitor& visitor) { visitor.visitLogicalExpr(this); }
void LiteralExpr::accept(Visitor& visitor) { visitor.visitLiteralExpr(this); }
void VariableExpr::accept(Visitor& visitor) { visitor.visitVariableExpr(this); }
void AssignExpr::accept(Visitor& visitor) { visitor.visitAssignExpr(this); }
void UnaryExpr::accept(Visitor& visitor) { visitor.visitUnaryExpr(this); }
void CallExpr::accept(Visitor& visitor) { visitor.visitCallExpr(this); }
void ArrayExpr::accept(Visitor& visitor) { visitor.visitArrayExpr(this); }
void MapExpr::accept(Visitor& visitor) { visitor.visitMapExpr(this); }
void IndexExpr::accept(Visitor& visitor) { visitor.visitIndexExpr(this); }
void IndexAssignExpr::accept(Visitor& visitor) { visitor.visitIndexAssignExpr(this); }

void ExpressionStmt::accept(Visitor& visitor) { visitor.visitExpressionStmt(this); }
void PrintStmt::accept(Visitor& visitor) { visitor.visitPrintStmt(this); }
void VarStmt::accept(Visitor& visitor) { visitor.visitVarStmt(this); }
void BlockStmt::accept(Visitor& visitor) { visitor.visitBlockStmt(this); }
void IfStmt::accept(Visitor& visitor) { visitor.visitIfStmt(this); }
void WhileStmt::accept(Visitor& visitor) { visitor.visitWhileStmt(this); }
void FunctionStmt::accept(Visitor& visitor) { visitor.visitFunctionStmt(this); }
void ReturnStmt::accept(Visitor& visitor) { visitor.visitReturnStmt(this); }
void ClassStmt::accept(Visitor& visitor) { visitor.visitClassStmt(this); }
void TryStmt::accept(Visitor& visitor) { visitor.visitTryStmt(this); }
void ThrowStmt::accept(Visitor& visitor) { visitor.visitThrowStmt(this); }
void ImportStmt::accept(Visitor& visitor) { visitor.visitImportStmt(this); }
void ForEachStmt::accept(Visitor& visitor) { visitor.visitForEachStmt(this); }
void BreakStmt::accept(Visitor& visitor)    { visitor.visitBreakStmt(this); }
void ContinueStmt::accept(Visitor& visitor) { visitor.visitContinueStmt(this); }
void TernaryExpr::accept(Visitor& visitor)  { visitor.visitTernaryExpr(this); }

void GetExpr::accept(Visitor& visitor) { visitor.visitGetExpr(this); }
void SetExpr::accept(Visitor& visitor) { visitor.visitSetExpr(this); }
void ThisExpr::accept(Visitor& visitor) { visitor.visitThisExpr(this); }
void SuperExpr::accept(Visitor& visitor) { visitor.visitSuperExpr(this); }
