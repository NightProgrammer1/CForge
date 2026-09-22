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
			
			IRExpr* operator()(const IRExprBinop& binary) const {
				auto left = tac.LowerExpr(binary.left);
				auto right = tac.LowerExpr(binary.right);

				auto temp = tac._allocator.new_object<IRExpr>(IRExprTemp{ ValueID::Create() }, expr->type);
				tac.Emit(TACBinop{
					temp,
					binary.op,
					left,
					right });

				return temp;
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

				tacLowering->Emit(jump);

			}
			void operator()(IRStatementCJump& cjump)
			{
				auto cond = tacLowering->LowerExpr(cjump.cond);

				tacLowering->Emit(
					IRStatementCJump{ cond, cjump.labelTrue, cjump.labelFalse }
				);

			}
			void operator()(IRStatementLabel& label)
			{
				tacLowering->Emit(label);

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
	
	IRExprTemp CreateTemp()
	{
		return IRExprTemp(Register::VReg{ ValueID::Create()});
	}

	std::pmr::polymorphic_allocator<Instruction>& _allocator;
	IRProgram& ir;

	TAC tac;
	std::unordered_map<std::string, size_t> variables;
	size_t temp_count = 1;
};