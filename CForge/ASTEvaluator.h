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
    ConstValue value; // Populated when status == Return
};



class ASTEvaluator {
public:
    ASTEvaluator(NodeProg& ast) : _ast(ast) {}
    // Evaluates an expression to a constant ConstValueeger ConstValue
    std::optional<ConstValue> EvaluateExpr(const NodeExpr* expr, std::unordered_map<std::string, ConstValue>& env) {
        if (!expr) return std::nullopt;

        struct Visitor {
            ASTEvaluator* evaluator;
            std::unordered_map<std::string, ConstValue>& env;

            std::optional<ConstValue> operator()(const NodeExprIntLit& lit) {
                return ConstValue { lit.value };
            }
            std::optional<ConstValue> operator()(const NodeExprBoolLit& lit) {
                return ConstValue{ lit.value };
            }
            std::optional<ConstValue> operator()(const NodeExprVoid& lit) {
                return {};
            }
            std::optional<ConstValue> operator()(const NodeExprIdent& var) {
                auto it = env.find(var.name);
                if (it != env.end()) return it->second;
                return std::nullopt; // Variable ConstValue not known at compile time
            }
            std::optional<ConstValue> operator()(const NodeExprBinary& bin) {
                auto left = evaluator->EvaluateExpr(bin.left, env);
                auto right = evaluator->EvaluateExpr(bin.right, env);
                if (!left || !right) return std::nullopt;

                return evaluator->EvaluateBinOp(bin.op.type, *left, *right);
            }
            std::optional<ConstValue> operator()(const NodeExprCompare& bin) {
                auto left = evaluator->EvaluateExpr(bin.left, env);
                auto right = evaluator->EvaluateExpr(bin.right, env);
                if (!left || !right) return std::nullopt;
               
                return evaluator->EvaluateBinOp(bin.op.type, *left, *right);

            }
            std::optional<ConstValue> operator()(const NodeExprUnary& un) {
                auto val = evaluator->EvaluateExpr(un.expr, env);
                if (!val || !val->is_scalar()) return std::nullopt;
                

                return std::visit([un](auto Val)->std::optional<ConstValue>
                    {
                        switch (un.op) {
                        case TokenType::post_increment: return ConstValue{ ++(Val) };
                        case TokenType::pre_increment: return ConstValue{ ++(Val) };
                        case TokenType::post_decrement: return ConstValue{ --(Val) };
                        case TokenType::pre_decrement:  return ConstValue{ --(Val) } ;
                        case TokenType::_not:          return ConstValue{ !(Val) };
                        case TokenType::negate:        return ConstValue{ -(Val) };

                        default: return std::nullopt;
                        }
                        return std::nullopt;
                    }, val->get_scalar());
                
            }
            std::optional<ConstValue> operator()(const NodeExprCall& call) {

                assert(call.func->body && "Function has no body");

                auto result = evaluator->EvaluateBlock(call.func->body.value(), env);

                if (result.status != EvalResult::Status::Return) return std::nullopt;

                return result.value;

            }
            std::optional<ConstValue> operator()(const NodeExprAssignment& assign) {
                auto val = evaluator->EvaluateExpr(assign.expr, env);
                if (!val) return std::nullopt;

                return val;
            }
            std::optional<ConstValue> operator()(const NodeExprInitList& assign)
            {
                return {};
            }
            std::optional<ConstValue> operator()(const NodeExprArrayAccess& assign)
            {
                return {};
            }
            std::optional<ConstValue> operator()(const NodeExprRef& ref)
            {
                return {};
            }
        };

        return std::visit(Visitor{ this, env }, expr->var);
    }

    EvalResult EvaluateBlock(const NodeBlock& block, std::unordered_map<std::string, ConstValue>& env) {
        for (const NodeStatement* stat : block.statements) {
            if (!stat) continue;

            EvalResult res = EvaluateStatement(stat, env);
            if (res.status != EvalResult::Status::Next) {
                return res;
            }
        }
        return { EvalResult::Status::Next, 0 };
    }
    EvalResult EvaluateWhile(const NodeStatementWhile& stat, std::unordered_map<std::string, ConstValue>& env)
    {
        NodeStatement stmt{ stat };
        return EvaluateStatement(&stmt, env);
    }
private:
    EvalResult EvaluateStatement(const NodeStatement* stat, std::unordered_map<std::string, ConstValue>& env) {
        struct Visitor {
            ASTEvaluator* evaluator;
            std::unordered_map<std::string, ConstValue>& env;


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
                    env[assign->name->name] = *val;
                }
                return { EvalResult::Status::Next };
            }
           
            EvalResult operator()(const NodeStatementVar& var) {
                auto val = evaluator->EvaluateExpr(var.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                env[var.name->name] = *val; 
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

                auto is_false = std::visit([](auto Val)->bool
                    {
                        return (Val == 0);
                    }, cond->get_scalar());
                if (!is_false) {
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

                    auto is_false = std::visit([](auto Val)->bool
                        {
                            return (Val == 0);
                        }, condVal->get_scalar());

                    if (is_false)
                    {
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
            EvalResult operator()(const NodeStatementArray& arr) {
                return { EvalResult::Status::Continue };
            }
           
        };

        return std::visit(Visitor{ this, env }, stat->stat);
    }

    std::optional<ConstValue> EvaluateBinOp(TokenType op, const ConstValue& lhs, const ConstValue& rhs) {
        // Binary operations only apply to scalar primitives
        if (!lhs.is_scalar() || !rhs.is_scalar()) {
            return std::nullopt;
        }

        return std::visit([op](auto L, auto R) -> std::optional<ConstValue> {
            using CommonType = std::common_type_t<decltype(L), decltype(R)>;
            auto l_promoted = static_cast<CommonType>(L);
            auto r_promoted = static_cast<CommonType>(R);

            switch (op) {
            case TokenType::plus:
                return ConstValue{ ConstValue::Scalar{ l_promoted + r_promoted } };
            case TokenType::sub:
                return ConstValue{ ConstValue::Scalar{ l_promoted - r_promoted } };
            case TokenType::star:
                return ConstValue{ ConstValue::Scalar{ l_promoted * r_promoted } };
            case TokenType::div:
                if (r_promoted == 0) return std::nullopt; 
                return ConstValue{ ConstValue::Scalar{ l_promoted / r_promoted } };
            case TokenType::less:
                return ConstValue{ ConstValue::Scalar{ l_promoted < r_promoted } };
            case TokenType::less_equal:
                return ConstValue{ ConstValue::Scalar{ l_promoted <= r_promoted } };
            case TokenType::greater:
                return ConstValue{ ConstValue::Scalar{ l_promoted > r_promoted } };
            case TokenType::greater_equal:
                return ConstValue{ ConstValue::Scalar{ l_promoted >= r_promoted } };
            case TokenType::equal_equal:
                return ConstValue{ ConstValue::Scalar{ l_promoted == r_promoted } };
            case TokenType::not_equal:
                return ConstValue{ ConstValue::Scalar{ l_promoted != r_promoted } };
			case TokenType::_and:
				return ConstValue{ ConstValue::Scalar{ l_promoted && r_promoted } };


            default:
                return std::nullopt;
            }
            }, lhs.get_scalar(), rhs.get_scalar());
    }
    NodeProg& _ast;
};