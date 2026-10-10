#pragma once
#include "IRTypes.h"





class TACLowering
{
public:

	TACLowering(IRProgram& mir, std::pmr::polymorphic_allocator<Instruction>& allocator) : ir(mir), _allocator(allocator) {}


	TACProgram Lower()
	{
		TACProgram tacProgram;

		for (auto& function : ir.functions)
		{
			for (auto stat : function.statements) {
				LowerStatement(*stat);
			}
			tac.name = function.name;
			tac.parameters = function.parameters;
			tac.stackSize = function.stackSize;
			tacProgram.functions.push_back(std::move(tac));
		}

		return std::move(tacProgram);

	}

private:
	IRExpr* LowerLogicalAnd(IRExpr* temp, IRExpr* left, IRExpr* right)
	{
		std::string labelFalse = GetFreshLabel("L_and_false_");
		std::string labelEnd = GetFreshLabel("L_and_end_");

		// Evaluate the left equation jump if false
		auto leftVal = LowerExpr(left);
		Emit(TACCJump{ leftVal, labelFalse });

		auto rightVal = LowerExpr(right);
		Emit(TACCJump{ rightVal, labelFalse });

		Emit(TACMove{ temp, MakeConst(1) });
		Emit(TACJump{ labelEnd });

		// False branch 
		Emit(TACLabel{ labelFalse });
		Emit(TACMove{ temp, MakeConst(0) });

		// Join Point
		Emit(TACLabel{ labelEnd });
		return temp;
	}

	IRExpr* LowerLogicalOr(IRExpr* temp, IRExpr* left, IRExpr* right)
	{
		std::string labelEvalRight = GetFreshLabel("L_or_right_");
		std::string labelBothFalse = GetFreshLabel("L_or_false_");
		std::string labelEnd = GetFreshLabel("L_or_end_");

		auto leftVal = LowerExpr(left);
		Emit(TACCJump{ leftVal, labelEvalRight });

		Emit(TACMove{ temp, MakeConst(1) });
		Emit(TACJump{ labelEnd });

		Emit(TACLabel{ labelEvalRight });
		auto rightVal = LowerExpr(right);

		Emit(TACCJump{ rightVal, labelBothFalse });

		Emit(TACMove{ temp, MakeConst(1) });
		Emit(TACJump{ labelEnd });

		Emit(TACLabel{ labelBothFalse });
		Emit(TACMove{ temp, MakeConst(0) });

		Emit(TACLabel{ labelEnd });
		return temp;
	}
	IRExpr* LowerExpr(IRExpr* expr)
	{

		struct ExprVisitor {
			TACLowering& tac;
			IRExpr* expr;


			IRExpr* operator()(const IRExprConst&) const {
				return expr;
			}

			IRExpr* operator()(const IRExprName&) const {
				return expr;
			}

			IRExpr* operator()(const IRExprTemp&) const {
				return expr;
			}
			
			IRExpr* operator()(const IRExprBinop& binop) const {
				

				auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, expr->type);

				return std::visit([this, &binop](auto&& op) -> IRExpr* {
					using T = std::decay_t<decltype(op)>;

					// 1. Short-Circuit Logical Operators (&&, ||)
					if constexpr (std::is_same_v<T, LogicalOp>) {
						auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, this->expr->type);
						if (op == LogicalOp::And) {
							return tac.LowerLogicalAnd(temp, binop.left, binop.right);
						}
						else {
							return tac.LowerLogicalOr(temp, binop.left, binop.right);
						}
					}

					// 2. Relational Comparison Operators (==, !=, <, etc.)
					else if constexpr (std::is_same_v<T, RelationalOp>) {
						auto leftVal = tac.LowerExpr(binop.left);
						auto rightVal = tac.LowerExpr(binop.right);

						auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, this->expr->type);
						// Emits sete/setl boolean result for register storage
						tac.Emit(TACBinop{ temp, op, leftVal, rightVal });
						return temp;
					}

					// 3. Eager Arithmetic & Bitwise Operators (+, -, *, etc.)
					else if constexpr (std::is_same_v<T, BinaryOp>) {
						auto leftVal = tac.LowerExpr(binop.left);
						auto rightVal = tac.LowerExpr(binop.right);

						auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, this->expr->type);
						tac.Emit(TACBinop{ temp, op, leftVal, rightVal });
						return temp;
					}
					}, binop.op);

			}

			IRExpr* operator()(const IRExprCall& call) const {
				TACCall callInst;

				callInst.function =
					std::get<IRExprName>(call.func->expr).name;

				for (auto* arg : call.args)
				{
					callInst.args.push_back(tac.LowerExpr(arg));
				}

				callInst.dst = call.dst;

				tac.Emit(callInst);

				return callInst.dst;
			}

			IRExpr* operator()(const IRExprMem& mem) const {
				auto address = tac.LowerExpr(mem.address);

				auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create()}, expr->type);

				tac.Emit(TACMove{ temp, address });
				return temp;
			}
			IRExpr* operator()(const IRExprDereference& deref) const {
				auto addressVal = tac.LowerExpr(deref.address);

				auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create()}, expr->type);

				tac.Emit(TACLoad{ temp, addressVal });

				return temp;
			}
			IRExpr* operator()(const IRExprAddressOf& address) const {
				auto object = tac.LowerExpr(address.object);

				auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, Type::MakePointer(address.object->type));

				tac.Emit(TACAddressOf{ temp, object });
				return temp;
			}

			IRExpr* operator()(const IRExprStackObject&) const {
				return expr;
			}
			IRExpr* operator()(const IRExprESEQ&) const {
				return expr;
			}
		};

		if (!expr) return nullptr;
		return std::visit(ExprVisitor{ *this, expr }, expr->expr);
	}



	IRExpr* MakeConst(const ConstValue::Scalar& value) const
	{
		return _allocator.new_object<IRExpr>(IRExprConst{ value });
	}
	IRExpr* LowerAddress(IRExpr* expr)
	{
		if (auto mem = std::get_if<IRExprMem>(&expr->expr))
		{
			return LowerExpr(mem->address);
		}

		return LowerExpr(expr);
	}
	void LowerStatement(IRStatement& stat)
	{

		struct Visitor
		{
			TACLowering* tacLowering;
			void operator()(IRStatementMove& move)
			{
				auto src = tacLowering->LowerExpr(move.src);

				if (auto deref = std::get_if<IRExprDereference>(&move.dst->expr))
				{
					auto address = tacLowering->LowerExpr(deref->address);

					tacLowering->Emit(
						TACMoveMem{ address, src } // Store 'src' into memory at 'address'
					);
				}
				else if (auto mem = std::get_if<IRExprMem>(&move.dst->expr))
				{
					auto address = tacLowering->LowerExpr(mem->address);

					tacLowering->Emit(
						TACMoveMem{ address, src }
					);
				}
				else
				{
					auto dst = tacLowering->LowerExpr(move.dst);

					tacLowering->Emit(
						TACMove{ dst, src }
					);
				}
			}
			void operator()(IRStatementExp& expr)
			{
				tacLowering->LowerExpr(expr.func);
			}
			void operator()(IRStatementJump& jump)
			{

				tacLowering->Emit(TACJump{jump.label});

			}
			void operator()(IRStatementCJump& cjump)
			{
				if (auto binop = std::get_if<IRExprBinop>(&cjump.cond->expr)) {

					if (binop->IsRelational()) {
						auto left = tacLowering->LowerExpr(binop->left);
						auto right = tacLowering->LowerExpr(binop->right);

						tacLowering->Emit(TACRelationalCJump{
							Invert(binop->GetRelationalOp()),       
							left,
							right,
							cjump.labelFalse
							});

						return;
					}
				}
				auto cond = tacLowering->LowerExpr(cjump.cond);
				tacLowering->Emit(TACCJump{ cond, cjump.labelFalse });
			}
			void operator()(IRStatementLabel& label)
			{
				tacLowering->Emit(TACLabel{label.label});

			}
			
			void operator()(IRStatementReturn& ret)
			{
				if (ret.value) {
					auto value = tacLowering->LowerExpr(ret.value);

					if (auto mem = std::get_if<IRExprMem>(&ret.value->expr))
					{
						auto dst = tacLowering->_allocator.new_object<IRExpr>(IRExprTemp{ Register::VReg{ValueID::Create()}}, ret.value->type);
						tacLowering->Emit(
							TACLoad{ dst, value }
						);
						tacLowering->Emit(TACReturn{ dst });
					}
					else {
						tacLowering->Emit(TACReturn{ value });
					}

				}
				else {
					tacLowering->Emit(TACReturn{ nullptr });
				}
			}
		} visitor{ this };

		std::visit(visitor, stat.stat);
	}


	void Emit(AnyInstuction&& expr)
	{
		tac.instructions.emplace_back(_allocator.new_object<Instruction>(expr));

	}
	
	bool IsRelationOp(const TokenType& op) const
	{
		return (op >= TokenType::less) && (op <=  TokenType::not_equal);
	}

	TokenType GetInvertedOp(const TokenType& op)
	{
		std::unordered_map<TokenType, TokenType> map
		{
			{TokenType::equal_equal, TokenType::not_equal },
			{ TokenType::not_equal, TokenType::equal_equal },
			{ TokenType::less, TokenType::greater },
			{ TokenType::greater, TokenType::less },
			{ TokenType::less_equal, TokenType::greater_equal },
			{ TokenType::greater_equal, TokenType::less_equal },


		};


		return map.at(op);
	}
	IRExprTemp CreateTemp()
	{
		return IRExprTemp(Register::VReg{ ValueID::Create()});
	}
	std::string GetFreshLabel(const std::string& label)
	{
		return label + std::to_string(label_counter++);
	}
	std::pmr::polymorphic_allocator<Instruction>& _allocator;
	IRProgram& ir;

	TAC tac;
	std::unordered_map<std::string, size_t> variables;
	size_t temp_count = 1;
	size_t label_counter = 0;

};