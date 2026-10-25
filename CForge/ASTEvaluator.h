#pragma once
#include "types.h"
#include "tokenization.h"
#include <unordered_map>
#include <optional>
#include <string>
#include <variant>
#include <cassert>


struct Scope
{
    std::unordered_map<size_t, ConstValue> variable;
};


// Control flow result wrapper
struct EvalResult {
    enum class Status { Next, Return, Break, Continue, Failure };
    Status status = Status::Next;
    ConstValue value; // Populated when status == Return
};



class ASTEvaluator {
public:
    ASTEvaluator(NodeProg& ast) {}
    // Evaluates an expression to a constant ConstValue
    static std::optional<ConstValue> EvaluateExpr(const NodeExpr* expr, std::vector<Scope>& env, ExprContext ctx = ExprContext::RValue) {
        if (!expr) return std::nullopt;

        struct Visitor {
            std::vector<Scope>& env;
            ExprContext ctx;
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
                if (auto value  = GetVariable(var.id, env))
                {
                    return *value;
                }
                return std::nullopt;
            }
            std::optional<ConstValue> operator()(const NodeExprBinary& bin) {
				return EvaluateBinOp(bin.op, bin.left, bin.right, env);

            }
            std::optional<ConstValue> operator()(const NodeExprCompare& bin) {
               auto result = EvaluateBinOp(bin.op, bin.left, bin.right, env);


               return result;
            }
            std::optional<ConstValue> operator()(const NodeExprUnary& un) {

                // Case 1: Taking the address of an array access (&arr[0])
                if (un.op == TokenType::ampersand) {

                    if (auto var = std::get_if<NodeExprIdent>(&un.expr->var))
                    {
						return ConstValue{ ConstValue::Pointer{ var->id } };
                    }
                    auto val =  EvaluateExpr(un.expr, env, ExprContext::LValue);

                    return val;
                }
                // Case 2: Dereferencing a pointer to read a value (*ptr)
                if (un.op == TokenType::star) {
                    auto ptrVal = EvaluateExpr(un.expr, env);

                    if (ctx == ExprContext::LValue) {
                        return ptrVal;
                    }

					if (ptrVal && ptrVal->is_pointer()) {
						const auto* ptr = ptrVal->get_pointer();
						if (auto arrayVal = GetVariable(ptr->variable_id, env)) {

							std::vector<size_t> finalIndices = ptr->indices;
							if (finalIndices.empty()) {
								finalIndices.push_back(ptr->element_offset);
							}
							else {
								finalIndices.back() += ptr->element_offset;
							}
							return *ConstValue::get_value_from_array(&*arrayVal, finalIndices);
						}
					}
                    return std::nullopt;
                }

                // Case 3: Standard variable increment/decrement/negation/not
                if (auto* ident = std::get_if<NodeExprIdent>(&un.expr->var)) {
                    auto var = GetVariable(ident->id, env);
                    if (!var)
                    {
                        return std::nullopt;
                    }
                    ConstValue originalVal = *var;
                    return std::visit(
                        [this, var, un](auto Val) -> std::optional<ConstValue> {
                            using T = decltype(Val);
                            if constexpr (std::is_same_v<T, bool>) {
                                switch (un.op) {
                                case TokenType::_not:
                                    return ConstValue{ ConstValue::Scalar{ !Val } };
                                default:
                                    return std::nullopt;
                                }
                            }
                            else {
                                T mutatedVal = Val;
                                T resultVal = Val;

                                switch (un.op) {

                                case TokenType::pre_increment:
                                    resultVal = ++mutatedVal;
                                    *var = ConstValue{ ConstValue::Scalar{ mutatedVal } };
                                    return ConstValue{ ConstValue::Scalar{ resultVal } };
                                case TokenType::post_increment:
                                    resultVal = mutatedVal++;
                                    *var = ConstValue{ ConstValue::Scalar{ mutatedVal } };
                                    return ConstValue{ ConstValue::Scalar{ resultVal } };
                                case TokenType::pre_decrement:
                                    resultVal = --mutatedVal;
                                    *var = ConstValue{ ConstValue::Scalar{ mutatedVal } };
                                    return ConstValue{ ConstValue::Scalar{ resultVal } };
                                case TokenType::post_decrement:
                                    resultVal = mutatedVal--;
                                    *var = ConstValue{ ConstValue::Scalar{ mutatedVal } };
                                    return ConstValue{ ConstValue::Scalar{ resultVal } };
                                case TokenType::ampersand:
                                    break;
                                case TokenType::_not:
                                    return ConstValue{ ConstValue::Scalar{ !Val } };
                                case TokenType::negate:
                                    if constexpr (std::is_same_v<T, bool>) {
                                        return std::nullopt;
                                    }
                                    else {
                                        return ConstValue{ ConstValue::Scalar{ -Val } };
                                    }
                                default:
                                    return std::nullopt;
                                }
                            }
                        },

                        originalVal.get_scalar()
                    );

                }

                // Fallback for general rvalue unary expressions
                auto val = EvaluateExpr(un.expr, env);
                if (!val || !val->is_scalar()) return std::nullopt;

                return std::visit(
                    [un](auto Val) -> std::optional<ConstValue> {
                        using T = decltype(Val);
                        switch (un.op) {
                        case TokenType::_not:
                            return ConstValue{ ConstValue::Scalar{ !Val } };
                        case TokenType::negate:
                            if constexpr (std::is_same_v<T, bool>) {
                                return std::nullopt;
                            }
                            else {
                                return ConstValue{ ConstValue::Scalar{ -Val } };
                            }
                        default:
                            return std::nullopt;
                        }
                    },
                    val->get_scalar()
                );
            }
            
            std::optional<ConstValue> operator()(const NodeExprCall& call) {

				std::vector<ConstValue> argValues;
				for (NodeExpr* arg : call.args) {
					auto foldedArg = EvaluateExpr(arg, env);
						argValues.push_back(*foldedArg);
				}
                assert(call.func->body && "Function has no body");



				EnterScope(env);

				for (size_t i = 0; i < call.func->parameters.size(); ++i) {

                    if (!GenerateVariable(call.func->parameters[i].id, argValues[i], env)) { 
                        std::cerr << "ConstFolder: Failed to generate const value for parameter: " << call.func->parameters[i].name << std::endl;
                        return std::nullopt; 
                    }

				}
                auto result = EvaluateBlock(call.func->body.value(), env);

				ExitScope(env);

                if (result.status != EvalResult::Status::Return)
                {
                    return std::nullopt;
                }
                return result.value;

            }
            std::optional<ConstValue> operator()(const NodeExprAssignment& assign) {
                auto val = EvaluateExpr(assign.expr, env);
                if (!val) return std::nullopt;

                auto id = GetID(assign.target);

                if (id == ValueID::INVALID_ID)
                {
                    return std::nullopt;
                }

                SetVariableValue(id, *val, env);
                return val;
            }
            std::optional<ConstValue> operator()(const NodeExprInitList& assign)
            {
                std::vector<ConstValue> arrayVal;
                for (auto value : assign.values)
                {
                    if (auto val = EvaluateExpr(value, env)) {
                        arrayVal.push_back(*val);
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }
                return ConstValue{ arrayVal };
            }
            std::optional<ConstValue> operator()(const NodeExprArrayAccess& access)
            {
				size_t arrayId = GetID(access.array);
				std::vector<size_t> indices;
				for (auto* idxExpr : access.indices) {
					if (auto val = EvaluateExpr(idxExpr, env)) {
						if (auto* s = std::get_if<int>(&val->get_scalar())) {
							indices.push_back(*s);
						}
					}
                    else {
                        return std::nullopt; // dynamic expr
                    }
				}


                if (ctx == ExprContext::LValue) {
                    return ConstValue{ ConstValue::Pointer{ arrayId, indices } };
                }

                if (auto val = GetVariable(arrayId, env)) { //RValue context

                    if (val->is_array()) {
                        if (auto* result = ConstValue::get_value_from_array(&*val, indices)) {
                            return *result;
                        }
                    }
                }
                return std::nullopt;
            }
          
        };

        return std::visit(Visitor{ env, ctx }, expr->var);
    }

    static EvalResult EvaluateBlock(const NodeBlock& block, std::vector<Scope>& env) {
        for (const NodeStatement* stat : block.statements) {
            if (!stat) continue;

            EvalResult res = EvaluateStatement(stat, env);
            if (res.status != EvalResult::Status::Next) {
                return res;
            }
        }
        return { EvalResult::Status::Next, 0 };
    }
    static EvalResult EvaluateWhile(const NodeStatementWhile& stat, std::vector<Scope>& env)
    {
        NodeStatement stmt{ stat };
        return EvaluateStatement(&stmt, env);
    }
private:
    static EvalResult EvaluateStatement(const NodeStatement* stat, std::vector<Scope>& env) {
        struct Visitor {
            std::vector<Scope>& env;
            ASTEvaluator* evaluator;

            EvalResult operator()(const NodeStatementExit& exit) {
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementExpr& stmt) {

				auto val = evaluator->EvaluateExpr(stmt.expr, env);
				if (!val) return { EvalResult::Status::Failure };

				if (auto assign = std::get_if<NodeExprAssignment>(&stmt.expr->var)) {
                    if (!GenerateVariable(GetID(stmt.expr), *val, env))
                    {
                        return { EvalResult::Status::Failure };
                    }
				}

				return { EvalResult::Status::Next };
            }
           
            EvalResult operator()(const NodeStatementVar& var) {

                auto val = evaluator->EvaluateExpr(var.initializer, env);
                if (!val) return { EvalResult::Status::Failure };

                if (env.empty()) env.emplace_back();
                env.back().variable[var.id] = *val;
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementAssignment& assign) {
				if (auto value = ASTEvaluator::EvaluateExpr(assign.expr, env)) {

					if (auto* ident = std::get_if<NodeExprIdent>(&assign.target->var)) {
						SetVariableValue(ident->id, *value, env);
                        return { EvalResult::Status::Next };
					}
					else if (auto lhs = ASTEvaluator::EvaluateExpr(assign.target, env, ExprContext::LValue)) {

						if (lhs->is_pointer())
						{
							auto ptr = lhs->get_pointer();
							if (auto varVal = GetVariable(ptr->variable_id, env))
							{
								if (varVal->is_scalar()) {
									SetVariableValue(ptr->variable_id, *value, env);
								}
								else if (varVal->is_array()) {
									ConstValue updatedArray = *varVal;
									std::vector<size_t> finalIndices = ptr->indices;
									if (finalIndices.empty()) {
										finalIndices.push_back(ptr->element_offset);
									}
									else {
										finalIndices.back() += ptr->element_offset;
									}
									updatedArray.set_value_at_indices(finalIndices, *value);
									SetVariableValue(ptr->variable_id, updatedArray, env);
									return { EvalResult::Status::Next };
								}
							}

						}
					}

				}

				return { EvalResult::Status::Failure };

            }
            EvalResult operator()(const NodeStatementReturn& ret) {
                auto val = EvaluateExpr(ret.expr, env);
                if (!val) return { EvalResult::Status::Failure };
                return { EvalResult::Status::Return, *val };
            }
            EvalResult operator()(const NodeBlock& block) {
                return EvaluateBlock(block, env);
            }
            EvalResult operator()(const NodeStatementIf& stmt) {
                auto cond = EvaluateExpr(stmt.condition, env);
                if (!cond) return { EvalResult::Status::Failure };

                auto is_false = std::visit([](auto Val)->bool
                    {
                        return (Val == 0);
                    }, cond->get_scalar());
                if (!is_false) {
                    return EvaluateStatement(stmt.thenBody, env);
                }
                else if (stmt.elseBody) {
                    return EvaluateStatement(stmt.elseBody, env);
                }
                return { EvalResult::Status::Next };
            }
            EvalResult operator()(const NodeStatementWhile& _while) {
                while (true) {
                    // Check condition
                    auto condVal = EvaluateExpr(_while.condition, env);
                    if (!condVal) return { EvalResult::Status::Failure }; // Dynamic expression

                    auto is_false = std::visit([](auto Val)->bool
                        {
                            return (Val == 0);
                        }, condVal->get_scalar());

                    if (is_false)
                    {
                        break;
                    }

                    EvalResult res = EvaluateStatement(_while.body, env);
                    

                    //Update
                    if (_while.update) {
                        auto updateVal = EvaluateExpr(_while.update, env);
                        if (!updateVal) return { EvalResult::Status::Failure }; // Dynamic expression
                    }
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
            EvalResult operator()(const NodeStatementBreak&) {
                return { EvalResult::Status::Break };
            }
            EvalResult operator()(const NodeStatementContinue&) {
                return { EvalResult::Status::Continue };
            }
            EvalResult operator()(const NodeStatementArray& arr) {
                if (!arr.initializer) {
                    size_t elementCount = arr.element_type->Size() / TypeInfo::GetBaseType(arr.element_type)->Size();

                    ConstValue defaultArray = ConstValue::create_array(elementCount);


                    if (env.empty()) env.emplace_back();
                    env.back().variable[arr.id] = defaultArray;
                    return { EvalResult::Status::Next };
                }


                if (auto init = EvaluateExpr(arr.initializer, env))
                {
                    if (env.empty()) env.emplace_back();
                    env.back().variable[arr.id] = *init;
					return { EvalResult::Status::Next };

                }
                return { EvalResult::Status::Failure };
            
            }
           
        };

        return std::visit(Visitor{ env }, stat->stat);
    }

    static std::optional<ConstValue> PointerAdd(const ConstValue::Pointer& lhs, const ConstValue& rhs)
    {
        assert(rhs.is_scalar());
		return std::visit([&lhs, &rhs](auto R)->std::optional<ConstValue> {
			using T = decltype(R);
			if constexpr (std::is_same_v<T, int>) {
				ConstValue::Pointer newPtr = lhs;
				newPtr.element_offset += R;
				return ConstValue{ newPtr };
			}
			else {
				return std::nullopt;
			}
			}, rhs.get_scalar());
    }
	static std::optional<ConstValue> PointerSub(const ConstValue::Pointer& lhs, const ConstValue& rhs)
	{
		assert(rhs.is_scalar());
		return std::visit([&lhs, &rhs](auto R)->std::optional<ConstValue> {
			using T = decltype(R);
			if constexpr (std::is_same_v<T, int>) {
				ConstValue::Pointer newPtr = lhs;
				newPtr.element_offset -= R;
				return ConstValue{ newPtr };
			}
			else {
				return std::nullopt;
			}
			}, rhs.get_scalar());
	}
	static std::optional<ConstValue> EvaluateBinOp(TokenType op, const NodeExpr* lhsExpr, const NodeExpr* rhsExpr, std::vector<Scope>& env)
	{

		auto lhsVal = EvaluateExpr(lhsExpr, env);
		auto rhsVal = EvaluateExpr(rhsExpr, env);
		if (!lhsVal || !rhsVal) return std::nullopt;
		auto is_pointer_or_array = [](const ConstValue& val) {
			return val.is_pointer() || val.is_array();
			};

		

		
		if (is_pointer_or_array(*rhsVal) && lhsVal->is_scalar()) {
			ConstValue ptrRhs = *rhsVal;
			if (rhsVal->is_array()) {
                size_t varId = GetID(rhsExpr);
				ptrRhs = {ConstValue::Pointer{ varId, {0}, 0 }};
			}

			switch (op) {
			case TokenType::plus:
				return PointerAdd(*ptrRhs.get_pointer(), *lhsVal);
			case TokenType::sub:
				return PointerSub(*ptrRhs.get_pointer(), *lhsVal);
			default:
				return std::nullopt;
			}
		}

		if (is_pointer_or_array(*lhsVal) && rhsVal->is_scalar()) {
			ConstValue ptrLhs = *lhsVal;
			if (lhsVal->is_array()) {
				size_t varId = GetID(lhsExpr);
                ptrLhs = { ConstValue::Pointer{ varId, {0}, 0 } };
			}

			switch (op) {
			case TokenType::plus:
				return PointerAdd(*ptrLhs.get_pointer(), *rhsVal);
			case TokenType::sub:
				return PointerSub(*ptrLhs.get_pointer(), *rhsVal);
			default:
				return std::nullopt;
			}
		}

		// Fall back to standard scalar operations if neither is a pointer/array
		if (!lhsVal->is_scalar() || !rhsVal->is_scalar()) {
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
			}, lhsVal->get_scalar(), rhsVal->get_scalar());
	}
    static void SetVariableValue(const size_t& ID, const ConstValue& value, std::vector<Scope>& constants)
    {
        if (ID == ValueID::INVALID_ID) return;

        auto find = std::find_if(
            constants.rbegin(), constants.rend(),
            [&](const Scope& s) {
                return s.variable.contains(ID);
            }
        );
        if (find != constants.rend())
        {
            find->variable.at(ID) = value;
        }
    }
    static ConstValue* GetVariable(const size_t ID, std::vector<Scope>& constants)
    {
        auto find = std::find_if(
            constants.rbegin(), constants.rend(),
            [&](const Scope& s) {
                return s.variable.contains(ID);
            }
        );

        if (find != constants.rend())
        {
            return &find->variable.at(ID);
        }
        
        return nullptr;
    }
	static bool GenerateVariable(const size_t& ID, const ConstValue& value, std::vector<Scope>& constants)
	{
		if (ID == ValueID::INVALID_ID) return false;
		else if (constants.empty()) { constants.emplace_back(); }
		constants.back().variable[ID] = value;


		return true;

	}

	static void EnterScope(std::vector<Scope>& constants) { constants.emplace_back(); }
	static void ExitScope(std::vector<Scope>& constants) { constants.pop_back(); }
    static size_t GetID(const NodeExpr* expr)
    {
        if (auto* identifier = std::get_if<NodeExprIdent>(&expr->var))
        {
            return identifier->id;
        }
        else if (auto* access = std::get_if<NodeExprArrayAccess>(&expr->var))
        {
            return GetID(access->array);
        }
        return ValueID::INVALID_ID;
    }


};