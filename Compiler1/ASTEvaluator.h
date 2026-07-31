#pragma once
#include "types.h"
#include "tokenization.h"
#include <unordered_map>
#include <optional>
#include <string>
#include <variant>
#include <cassert>

// Control flow result wrapper
struct EvalResult {
    enum class Status { Next, Return, Break, Continue, Failure };
    Status status = Status::Next;
    int value = 0; // Populated when status == Return
};

class ASTEvaluator {
public:
    ASTEvaluator(NodeProg& ast) : _ast(ast) {}
    // Evaluates an expression to a constant integer value
    std::optional<int> EvaluateExpr(const NodeExpr* expr, std::unordered_map<std::string, int>& env) {
        if (!expr) return std::nullopt;

        struct Visitor {
            ASTEvaluator* evaluator;
            std::unordered_map<std::string, int>& env;

            std::optional<int> operator()(const NodeExprIntLit& lit) {
                return lit.value;
            }
            std::optional<int> operator()(const NodeExprBoolLit& lit) {
                return lit.value;
            }
            std::optional<int> operator()(const NodeExprVoid& lit) {
                return {};
            }
            std::optional<int> operator()(const NodeExprIdent& var) {
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
                auto left = evaluator->EvaluateExpr(bin.left, env);
                auto right = evaluator->EvaluateExpr(bin.right, env);
                if (!left || !right) return std::nullopt;
                switch (bin.op.type) {
                case TokenType::less:  return *left < *right;
                case TokenType::greater: return *left > *right;
                case TokenType::equal_equal: return *left == *right;
                case TokenType::less_equal: return *left <= *right;
                case TokenType::greater_equal: return *left >= *right;
                case TokenType::not_equal: return *left != *right;
                default: return std::nullopt;
                }

            }
            std::optional<int> operator()(const NodeExprUnary& un) {
                auto val = evaluator->EvaluateExpr(un.expr, env);
                if (!val) return std::nullopt;

                switch (un.op.type) {
                case TokenType::post_increment: return ++(*val);
                case TokenType::pre_increment : return ++(*val);
                case TokenType::post_decrease : return --(*val);
                case TokenType::pre_decrease  : return --(*val);


                default: return std::nullopt;
                }
            }
            std::optional<int> operator()(const NodeExprCall& call) {

                assert(call.func->body && "Function has no body");

                auto result = evaluator->EvaluateBlock(call.func->body.value(), env);

                if (result.status != EvalResult::Status::Return) return std::nullopt;

                return result.value;

            }
            std::optional<int> operator()(const NodeExprAssignment& assign) {
                auto val = evaluator->EvaluateExpr(assign.expr, env);
                if (!val) return std::nullopt;

                return val;
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
    EvalResult EvaluateWhile(const NodeStatementWhile& stat, std::unordered_map<std::string, int>& env)
    {
        NodeStatement stmt{ stat };
        return EvaluateStatement(&stmt, env);
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
                auto val = evaluator->EvaluateExpr(exit.expr, env);
                if (!val) return { EvalResult::Status::Failure };

                if (auto un = std::get_if<NodeExprUnary>(&exit.expr->var)) {
                    auto var = std::get_if<NodeExprIdent>(&un->expr->var);
                    env[var->name] = *val;
                }
                else if (auto assign = std::get_if<NodeExprAssignment>(&exit.expr->var)) {
                    env[assign->name] = *val;
                }
                return { EvalResult::Status::Next };
            }
           
            EvalResult operator()(const NodeStatementVar& var) {
                auto val = evaluator->EvaluateExpr(var.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                env[var.name] = *val; 
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementAssignment& assign) {
                auto val = evaluator->EvaluateExpr(assign.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                env[assign.name] = *val; 
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
            EvalResult operator()(const NodeStatementWhile& _while) {
                while (true) {
                    // 1. Check condition
                    auto condVal = evaluator->EvaluateExpr(_while.condition, env);
                    if (!condVal) return { EvalResult::Status::Failure }; // Dynamic expression

                    if (*condVal == 0) {
                        break; 
                    }

                    EvalResult res = evaluator->EvaluateBlock(_while.block, env);

                    if (res.status == EvalResult::Status::Return || res.status == EvalResult::Status::Failure) {
                        return res; 
                    }

                    if (res.status == EvalResult::Status::Break) {
                        break; 
                    }

                    if (res.status == EvalResult::Status::Continue) {
                        continue; 
                    }
                }

                return { EvalResult::Status::Next, 0 };
            }
            EvalResult operator()(const NodeStatementBreak& _break) {
                return { EvalResult::Status::Break };
            }
            EvalResult operator()(const NodeStatementContinue& _continue) {
                return { EvalResult::Status::Continue };
            }
           
        };

        return std::visit(Visitor{ this, env }, stat->stat);
    }

    NodeProg& _ast;
};