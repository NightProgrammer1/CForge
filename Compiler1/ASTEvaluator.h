#pragma once
#include "types.h"
#include "tokenization.h"
#include <unordered_map>
#include <optional>
#include <string>
#include <variant>

// Control flow result wrapper
struct EvalResult {
    enum class Status { Next, Return, Break, Continue, Failure };
    Status status = Status::Next;
    int value = 0; // Populated when status == Return
};

class ASTEvaluator {
public:
    // Evaluates an expression to a constant integer value
    std::optional<int> EvaluateExpr(const NodeExpr* expr, const std::unordered_map<std::string, int>& env) {
        if (!expr) return std::nullopt;

        struct Visitor {
            ASTEvaluator* evaluator;
            const std::unordered_map<std::string, int>& env;

            std::optional<int> operator()(const NodeExprIntLit& lit) {
                return lit.value;
            }
            std::optional<int> operator()(const NodeExprBoolLit& lit) {
                return lit.value;
            }
            std::optional<int> operator()(const NodeExprVoid& lit) {
                return {};
            }
            std::optional<int> operator()(const NodeExprIndent& var) {
                auto it = env.find(var.name);
                if (it != env.end()) return it->second;
                return std::nullopt; // Variable value not known at compile time
            }
            std::optional<int> operator()(const NodeExprBinary& bin) {
                auto left = evaluator->EvaluateExpr(bin.left, env);
                auto right = evaluator->EvaluateExpr(bin.right, env);
                if (!left || !right) return std::nullopt;

                switch (bin.op.type) {
                case TokenType::plus:  return *left + *right;
                case TokenType::sub: return *left - *right;
                case TokenType::mult:  return *left * *right;
                case TokenType::div: return (*right != 0) ? std::optional<int>(*left / *right) : std::nullopt;
                default: return std::nullopt;
                }
            }
            std::optional<int> operator()(const NodeExprCompare& bin) {
                return {};
            }
            std::optional<int> operator()(const NodeExprUnary& un) {
                auto val = evaluator->EvaluateExpr(un.expr, env);
                if (!val) return std::nullopt;
                if (un.op.type == TokenType::sub) return -(*val);
                return std::nullopt;
            }
            std::optional<int> operator()(const NodeExprCall& call) {
                return {};
            }
        };

        return std::visit(Visitor{ this, env }, expr->var);
    }

    EvalResult EvaluateBlock(const NodeBlock& block, std::unordered_map<std::string, int>& env) {
        for (const NodeStatement* stat : block.statements) {
            if (!stat) continue;

            EvalResult res = EvaluateStatement(stat, env);
            if (res.status != EvalResult::Status::Next) {
                return res;
            }
        }
        return { EvalResult::Status::Next, 0 };
    }

private:
    EvalResult EvaluateStatement(const NodeStatement* stat, std::unordered_map<std::string, int>& env) {
        struct Visitor {
            ASTEvaluator* evaluator;
            std::unordered_map<std::string, int>& env;


            EvalResult operator()(const NodeStatementExit& exit) {
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementExpr& exit) {
                return { EvalResult::Status::Next };
            }
           
            EvalResult operator()(const NodeStatementVar& var) {
                auto val = evaluator->EvaluateExpr(var.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                env[var.name] = *val; // Bind local variable
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementAssignment& assign) {
                auto val = evaluator->EvaluateExpr(assign.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                env[assign.ident.value] = *val; // Update local variable
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementReturn& ret) {
                auto val = evaluator->EvaluateExpr(ret.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                return { EvalResult::Status::Return, *val };
            }
            EvalResult operator()(const NodeBlock& block) {
                return evaluator->EvaluateBlock(block, env);
            }
            EvalResult operator()(const NodeStatementIf& stmt) {
                auto cond = evaluator->EvaluateExpr(stmt.condition, env);
                if (!cond) return { EvalResult::Status::Failure };

                if (*cond != 0) {
                    return evaluator->EvaluateBlock(stmt.thenBlock, env);
                }
                else if (stmt.elseBlock) {
                    return evaluator->EvaluateBlock(*stmt.elseBlock, env);
                }
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementWhile& exit) {
                return { EvalResult::Status::Next };
            }
           
        };

        return std::visit(Visitor{ this, env }, stat->stat);
    }
};