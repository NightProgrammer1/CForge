#pragma once
#include "IRTypes.h"
#include <cassert>
#include "SymbolTable.h"
class IRLowering
{
public:

	IRLowering(const NodeProg& program, std::pmr::polymorphic_allocator<NodeExpr>& allocator) : ast(program), _allocator(allocator){}


	IRProgram Lower()
	{
		IRProgram irTree;


		for (const auto& stat : ast.global_space.statements) {
			statements.clear();
			LowerStatement(*stat);
		}
		for (const auto& func : ast.functions)
		{
			statements.clear();
			IRFunction irFunction;

			irFunction.name = func.name;
			irFunction.returnType = func.retType;
			irFunction.stackSize = func.stack_size;
			for (const auto& parameter : func.parameters)
			{
				irFunction.parameters.push_back(Register::VReg(parameter.id, TypeInfo::GetTypeSize(*parameter.type)));
			}
			for (const auto& stat : func.body->statements) {
				LowerStatement(*stat);
			}


			if (TypeInfo::IsVoid(func.retType))
			{
				Emit(IRStatementReturn{ nullptr });

			}
			irFunction.statements = std::move(statements);
			irTree.functions.push_back(std::move(irFunction));
		}


		return irTree;
	}
private:
	
	IRExpr* LowerExpr(const NodeExpr& expr, ExprContext ctx = ExprContext::RValue)
	{

		
		struct Visitor
		{
			IRLowering* irLowering;
			const NodeExpr& expr;
			ExprContext ctx;
			IRExpr* operator()(const NodeExprIntLit& intLit)
			{
				return irLowering->_allocator.new_object<IRExpr>( IRExprConst{ intLit.value } );
			}
			IRExpr* operator()(const NodeExprBoolLit& boolLit)
			{
				return irLowering->_allocator.new_object<IRExpr>(IRExprConst{ static_cast<int>(boolLit.value) });
			}
			IRExpr* operator()(const NodeExprIdent& ident)
			{

				if (ident.symbol->stackOffset == 0)
				{
					return irLowering->GetTemp(ident.id, ident.type);
				}


				auto type = ident.symbol->type;
				Register::StackObject object;
				object.id = ident.symbol->id;
				object.size = TypeInfo::GetTypeSize(*type);
				object.alignment = TypeInfo::GetTypeAlignment(*type);
				object.offset = ident.symbol->stackOffset;


				if (TypeInfo::IsArray(ident.type))
				{
					auto stackObj = irLowering->_allocator.new_object<IRExpr>(IRExprStackObject{ object }, type);
					return irLowering->_allocator.new_object<IRExpr>(
						IRExprAddressOf{ stackObj }, type
						);
				}
				

				
				return irLowering->_allocator.new_object<IRExpr>(IRExprStackObject{ object }, type);

			}
			IRExpr* operator()(const NodeExprVoid& _void)
			{

				return nullptr;
			}
			IRExpr* operator()(const NodeExprBinary& binary)
			{
				
				auto left = irLowering->LowerExpr(*binary.left);
				auto right = irLowering->LowerExpr(*binary.right);

				auto leftInfo = binary.left->type;
				auto rightInfo = binary.right->type;

				bool leftPointer = TypeInfo::IsPointer(leftInfo);
				bool rightPointer = TypeInfo::IsPointer(rightInfo);

				if (binary.op == TokenType::plus)
				{
					if (leftPointer && !rightPointer)
					{
						if (auto baseType = leftInfo->element)
						{
							int elemSize = static_cast<int>(TypeInfo::GetTypeSize(*baseType));
							auto sizeExpr = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ elemSize });
							right = irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(TokenType::star), right, sizeExpr }, expr.type);
						}
					}
					else if (!leftPointer && rightPointer)
					{
						if (auto baseType = rightInfo->element)
						{
							int elemSize = static_cast<int>(TypeInfo::GetTypeSize(*baseType));
							auto sizeExpr = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ elemSize });
							left = irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(TokenType::star), left, sizeExpr }, expr.type);
						}
					}

				}
				else if (binary.op == TokenType::sub)
				{
					if (leftPointer && !rightPointer) {
						if (auto baseType = leftInfo->element)
						{
							int elemSize = static_cast<int>(TypeInfo::GetTypeSize(*baseType));
							auto sizeExpr = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ elemSize });
							right = irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(TokenType::star), right, sizeExpr }, expr.type);
						}
					}
					else if (leftPointer && rightPointer)
					{
						// Emit (left - right) / sizeof(*left)
						auto diff = irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(TokenType::sub), left, right }, expr.type);
						if (auto baseType = TypeInfo::GetBaseType(leftInfo))
						{
							int elemSize = static_cast<int>(TypeInfo::GetTypeSize(*leftInfo->element));
							auto sizeExpr = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ elemSize });
							return irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(TokenType::div), diff, sizeExpr }, expr.type);
						}
					}
				}
				

				return irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ TokenToBinaryOp(binary.op), left, right }, expr.type);
			}
			IRExpr* operator()(const NodeExprCompare& comp)
			{
				auto left = irLowering->LowerExpr(*comp.left);
				auto right = irLowering->LowerExpr(*comp.right);

				return irLowering->_allocator.new_object<IRExpr>(IRExprBinop{ ConvertToIROp(comp.op), left, right }, expr.type);
			}
			IRExpr* operator()(const NodeExprCall& funcCall)
			{
				IRExprCall call;
				auto name = irLowering->_allocator.new_object<IRExpr>(IRExprName{ funcCall.name });
				call.func = name;
				for (auto& arg : funcCall.args)
				{
					auto loweredArg = irLowering->LowerExpr(*arg);
					
					call.args.push_back(loweredArg);
				}
				bool isVoid = funcCall.func->retType && TypeInfo::IsVoid(funcCall.func->retType);
				if (isVoid)
				{
					// Void function: emit direct statement call with no destination
					auto callExpr = irLowering->_allocator.new_object<IRExpr>(call);
					irLowering->Emit(IRStatementExp{ callExpr });
					return nullptr;
				}
				auto resultTemp = irLowering->_allocator.new_object<IRExpr>(
					IRExprTemp{ Register::VReg{ ValueID::Create()}}, funcCall.func->retType
				);

				// Bind call destination to the result temporary
				call.dst = resultTemp;

				auto callExpr = irLowering->_allocator.new_object<IRExpr>(call);
				irLowering->Emit(IRStatementExp{ callExpr });

				return resultTemp;
				
			}
			IRExpr* operator()(const NodeExprUnary& un)
			{
				switch (un.op)
				{
				case TokenType::ampersand:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::LValue);

					if (auto mem = std::get_if<IRExprMem>(&target->expr)) {
						return mem->address;
					}
					return irLowering->_allocator.new_object<IRExpr>(IRExprAddressOf{ target });
				}
				case TokenType::star:
				{
					auto address = irLowering->LowerExpr(*un.expr);
					auto exprType = expr.type;


					IRExpr* result = nullptr;
					if (exprType && TypeInfo::IsPointer(exprType) && exprType->element)
					{
						if (TypeInfo::IsArray(exprType->element))
						{
							// Dereferencing a pointer-to-array yields an array lvalue.
							return address;
						}

					}

					if (ctx == ExprContext::RValue)
					{
						result = irLowering->_allocator.new_object<IRExpr>(IRExprDereference{ address }, exprType);
					}
					else
					{
						result = irLowering->_allocator.new_object<IRExpr>(IRExprMem{ address }, exprType);
					}
					return result;
					break;
				}
				case TokenType::post_increment:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::LValue);

					size_t typeSize = TypeInfo::GetTypeSize(*un.expr->type);

					IRExpr* oldTemp = irLowering->CreateTemp(typeSize);

					IRExpr* stride = nullptr;
					if (TypeInfo::IsPointer(un.expr->type)) {
						size_t elementSize = TypeInfo::GetTypeSize(*un.expr->type->element);
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ static_cast<int>(elementSize) });
					}
					else {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ 1 });
					}

					irLowering->Emit(IRStatementMove{ oldTemp, target });

					auto calculation = IRExprBinop{ BinaryOp::Add, oldTemp, stride };
					auto calcExpr = irLowering->_allocator.new_object<IRExpr>(calculation, un.expr->type);

					irLowering->Emit(IRStatementMove{ target, calcExpr });

					return oldTemp;
					break;

				}
				case TokenType::pre_increment:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::LValue);
					IRExpr* stride = nullptr;
					size_t elementSize = TypeInfo::GetTypeSize(*un.expr->type->element);
					if (TypeInfo::IsPointer(un.expr->type)) {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ static_cast<int>(elementSize) });
					}
					else {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ 1 });
					}

					auto calculation = IRExprBinop{ BinaryOp::Add, target, stride };
					auto calcExpr = irLowering->_allocator.new_object<IRExpr>(calculation);

					irLowering->Emit(IRStatementMove{ target, calcExpr });

					return target;
					break;

				}
				case TokenType::pre_decrement:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::LValue);
					IRExpr* stride = nullptr;
					size_t elementSize = TypeInfo::GetTypeSize(*un.expr->type->element);
					if (TypeInfo::IsPointer(un.expr->type)) {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ static_cast<int>(elementSize) });
					}
					else {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ 1 });
					}

					auto calculation = IRExprBinop{ BinaryOp::Sub, target, stride };
					auto calcExpr = irLowering->_allocator.new_object<IRExpr>(calculation);

					irLowering->Emit(IRStatementMove{ target, calcExpr });

					return target;
					break;

				}
				case TokenType::post_decrement:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::LValue);

					size_t typeSize = TypeInfo::GetTypeSize(*un.expr->type);

					IRExpr* oldTemp = irLowering->CreateTemp(typeSize);

					IRExpr* stride = nullptr;
					size_t elementSize = TypeInfo::GetTypeSize(*un.expr->type->element);
					if (TypeInfo::IsPointer(un.expr->type)) {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ static_cast<int>(elementSize) }, Type::MakeInt());
					}
					else {
						stride = irLowering->_allocator.new_object<IRExpr>(IRExprConst{ 1 }, Type::MakeInt());
					}

					irLowering->Emit(IRStatementMove{ oldTemp, target });

					auto calculation = IRExprBinop{ BinaryOp::Sub, oldTemp, stride };
					auto calcExpr = irLowering->_allocator.new_object<IRExpr>(calculation);

					irLowering->Emit(IRStatementMove{ target, calcExpr });

					return oldTemp;
					break;

				}
				case TokenType::_not:
				{
					auto target = irLowering->LowerExpr(*un.expr, ExprContext::RValue);
					auto zero = irLowering->MakeConst(0); 

					return irLowering->_allocator.new_object<IRExpr>(
						IRExprBinop{ RelationalOp::Equal, target, zero },
						expr.type
					);
				}
				default:
					return nullptr;
					break;
				}
				return nullptr;
			}
			IRExpr* operator()(const NodeExprAssignment& assign)
			{
				IRExpr* src = irLowering->LowerExpr(*assign.expr);

				IRExpr* dst = irLowering->LowerExpr(*assign.target);

				auto move = irLowering->_allocator.new_object<IRStatement>(
					IRStatementMove{ dst, src }
				);
				irLowering->Emit(move);

				return dst;
			}
			IRExpr* operator()(const NodeExprInitList& list)
			{
				for (auto& element : list.values)
				{
					if (std::holds_alternative<NodeExprInitList>(element->var)) {
						irLowering->LowerExpr(*element);
					}
					else {
						auto dst = irLowering->_allocator.new_object<IRExpr>(IRExprTemp{ Register::VReg{.id = ValueID::Create()} });
						auto src = irLowering->LowerExpr(*element);
						irLowering->Emit(IRStatementMove{ dst, src });
					}
				}
				return nullptr;
			}
			IRExpr* operator()(const NodeExprArrayAccess& access)
			{
				auto arrayExpr = access.array;

				auto array = irLowering->LowerExpr(*arrayExpr);
				auto size = TypeInfo::GetBaseType(arrayExpr->type)->Size();
				IRExpr* base = array;


				IRExpr* totalOffset = nullptr;

				for (size_t i = 0; i < access.indices.size(); ++i)
				{
					auto* index = irLowering->LowerExpr(*access.indices[i]);
					size_t stride = irLowering->GetArrayStride(access, i);

					IRExpr* dimOffset = nullptr;
					if (stride == 1) {
						dimOffset = index;
					}
					else {
						auto* strideExpr = irLowering->_allocator.new_object<IRExpr>(
							IRExprConst{ static_cast<size_t>(stride) }, Type::MakeInt()
						);

						dimOffset = irLowering->_allocator.new_object<IRExpr>(
							IRExprBinop{ TokenToBinaryOp(TokenType::star), index, strideExpr }, Type::MakePointer(Type::MakeInt())
						);
					}

					if (!totalOffset) {
						totalOffset = dimOffset;
					}
					else {
						totalOffset = irLowering->_allocator.new_object<IRExpr>(
							IRExprBinop{ TokenToBinaryOp(TokenType::plus), totalOffset, dimOffset }, Type::MakePointer(Type::MakeInt())
						);
					}
				}
				if (totalOffset)
				{
					base = irLowering->_allocator.new_object<IRExpr>(
						IRExprBinop{ TokenToBinaryOp(TokenType::plus), base, totalOffset }, Type::MakePointer(Type::MakeInt())
					);
				}
				size_t totalDimensions = TypeInfo::GetArrayDimensions(arrayExpr->type).size();
				if (access.indices.size() < totalDimensions)
				{
					return base;
				}

				size_t elementSize = TypeInfo::GetTypeSize(*access.array->type);


				if (ctx == ExprContext::RValue) {
					base = irLowering->_allocator.new_object<IRExpr>(
						IRExprDereference{ base }, TypeInfo::GetBaseType(arrayExpr->type)
					);
				}
				else
				{
					base = irLowering->_allocator.new_object<IRExpr>(
						IRExprMem{ base }, TypeInfo::GetBaseType(arrayExpr->type)
					);
				}

				return base;
			}
			IRExpr* operator()(const NodeExprRef& ref)
			{
				return nullptr;
			}
		} visitor{ this, expr, ctx  };

		return std::visit(visitor, expr.var);
		
	}



	void LowerStatement(NodeStatement& stat)
	{

		struct Visitor 
		{
			IRLowering* irLowering;

			void operator()(NodeStatementExit& exit) {

			}
			void operator()(NodeStatementVar& var)
			{

				// All variables are stored directly into stack
				Register::StackObject stackObject;
				stackObject.id = var.id;
				stackObject.size = TypeInfo::GetTypeSize(*var.element_type);
				stackObject.alignment = TypeInfo::GetTypeAlignment(*var.element_type);
				stackObject.offset = var.symbol->stackOffset;

				auto dst = irLowering->_allocator.new_object<IRExpr>(
					IRExprStackObject{ stackObject }, var.element_type
				);
				if (var.initializer) {
					auto src = irLowering->LowerExpr(*var.initializer);

					auto move = irLowering->_allocator.new_object<IRStatement>(
						IRStatementMove{ dst, src }
					);
					irLowering->Emit(move);
				}
				else {
					// Optional: Emit default zero-initialization or just register allocation
					// depending on your language semantics (e.g., dst = 0)
				}
			}
			void operator()(NodeBlock& block)
			{
				for (auto& stat : block.statements)
				{
					irLowering->LowerStatement(*stat);
				}
			}
			void operator()(NodeStatementIf& ifStmt)
			{
				std::string labelTrue = irLowering->GetFreshLabel("if_true_");
				std::string labelFalse = irLowering->GetFreshLabel("if_false_");
				std::string labelEnd = irLowering->GetFreshLabel("if_end_");

				auto condVal = irLowering->LowerExpr(*ifStmt.condition);

				irLowering->Emit(IRStatementCJump{ condVal, ifStmt.elseBody ? labelFalse : labelEnd});

				irLowering->Emit(IRStatementLabel{ labelTrue });
				irLowering->LowerStatement(*ifStmt.thenBody);
				irLowering->Emit(IRStatementJump{ labelEnd });

				if (ifStmt.elseBody) {
					irLowering->Emit(IRStatementLabel{ labelFalse });
					auto elseB = NodeStatement{ *ifStmt.elseBody };
					irLowering->LowerStatement(elseB);
				}

				irLowering->Emit(IRStatementLabel{ labelEnd });

			}
			void operator()(NodeStatementWhile& _while)
			{
				std::string condLabel =
					irLowering->GetFreshLabel("while_cond");

				std::string bodyLabel =
					irLowering->GetFreshLabel("while_body");

				std::string endLabel =
					irLowering->GetFreshLabel("while_end");
				std::string labelStep = 
					irLowering->GetFreshLabel("for_step_");

				// Condition block
				irLowering->Emit(IRStatementLabel{ condLabel });

				auto cond = irLowering->LowerExpr(*_while.condition);
				irLowering->Emit(IRStatementCJump{
							cond,
							endLabel
					});
				// Body
				irLowering->Emit(IRStatementLabel{ bodyLabel });

				irLowering->PushLoop(_while.update ? labelStep : condLabel, endLabel);

				irLowering->LowerStatement(*_while.body);

				if (_while.update) {
					irLowering->Emit(IRStatementLabel{ labelStep });
					irLowering->LowerExpr(*_while.update);
				}
				irLowering->Emit(IRStatementJump{ condLabel });

				// Exit
				irLowering->Emit(IRStatementLabel{ endLabel });
				irLowering->PopLoop();
			}
			void operator()(NodeStatementBreak& _break)
			{
				assert(!irLowering->breakLabels.empty() && "Break outside of loop");
				irLowering->Emit(IRStatementJump{ irLowering->breakLabels.back() });
			}
			void operator()(NodeStatementContinue& _continue)
			{
				assert(!irLowering->continueLabels.empty() && "Continue outside of loop");
				irLowering->Emit(IRStatementJump{ irLowering->continueLabels.back() });

			}
			void operator()(NodeStatementExpr& expr)
			{
				auto expression = irLowering->LowerExpr(*expr.expr);

			}
			void operator()(NodeStatementReturn& _return)
			{
				auto src = irLowering->LowerExpr(*_return.expr);

				irLowering->Emit(IRStatementReturn{ src });
			}
			void operator()(NodeStatementAssignment& assign)
			{
				auto src = irLowering->LowerExpr(*assign.expr);
				auto dst = irLowering->LowerExpr(*assign.target, ExprContext::LValue);

				irLowering->Emit(IRStatementMove{ dst, src });
			}
			void operator()(NodeStatementArray& arr)
			{
				Register::StackObject a;
				a.id = arr.id;
				a.size = TypeInfo::GetTotalArraySize(arr.element_type);
				a.alignment = TypeInfo::GetTypeAlignment(*arr.element_type);
				a.offset = arr.symbol->stackOffset;

				auto stackObject =
					irLowering->_allocator.new_object<IRExpr>(
						IRExprStackObject{ a }, arr.element_type
					);
				
				if(arr.initializer) {
					size_t offset = 0;
					irLowering->LowerArrayInitializer(
						stackObject,
						*arr.initializer,
						offset,
						TypeInfo::GetTypeSize(*TypeInfo::GetBaseType(arr.element_type))
					);
				}
			}
		} visitor{ this };

		std::visit(visitor, stat.stat);
	}
	
	
	void LowerArrayInitializer(
		IRExpr* base,
		NodeExpr& initializer,
		size_t& offset,
		size_t elementSize)
	{
		if (auto list = std::get_if<NodeExprInitList>(&initializer.var))
		{
			for (auto& element : list->values)
			{
				LowerArrayInitializer(
					base,
					*element,
					offset,
					elementSize
				);
			}

			return;
		}

		auto address = _allocator.new_object<IRExpr>(
			IRExprAddressOf{ base }, Type::MakePointer(Type::MakeInt())
		);

		auto offsetExpr = _allocator.new_object<IRExpr>(
			IRExprConst{
				ConstValue::Scalar{
					static_cast<int>(offset)
				}
			}, Type::MakeInt()
		);

		auto elementAddress =
			_allocator.new_object<IRExpr>(
				IRExprBinop{
					TokenToBinaryOp(TokenType::plus),
					address,
					offsetExpr,
				}, Type::MakePointer(Type::MakeInt())
			);

		auto destination =
			_allocator.new_object<IRExpr>(
				IRExprMem{ elementAddress }, Type::MakePointer(Type::MakeInt())
			);

		auto src = LowerExpr(initializer);

		Emit(
			IRStatementMove{ destination, src }
		);

		offset += elementSize;
	}
	size_t GetArrayStride(const NodeExprArrayAccess& access, size_t dim)
	{
		const auto& arrayType = access.array->type;
		auto arrayDimensions = TypeInfo::GetArrayDimensions(arrayType);

		if (dim >= arrayDimensions.size()) {
			auto elementType = arrayType->element;
			return TypeInfo::GetTypeSize(*elementType);
		}

		auto scalarType = TypeInfo::GetBaseType(arrayType);
		size_t stride = TypeInfo::GetTypeSize(*scalarType);

		
		for (size_t i = dim + 1; i < arrayDimensions.size(); ++i) {
			stride *= arrayDimensions[i];
		}

		return stride;
	}
	IRExpr* GetTemp(const size_t& id, const std::shared_ptr<Type>& type)
	{
		auto vreg = Register::VReg{ id };
		return _allocator.new_object<IRExpr>(IRExprTemp{ vreg }, type);
	}
	IRExpr* CreateTemp(const size_t& size)
	{
		auto vreg = Register::VReg{ValueID::Create(), size};
		return _allocator.new_object<IRExpr>(IRExprTemp{vreg});
	}
	void Emit(AnyIRStatement&& statement)
	{
		statements.emplace_back(_allocator.new_object<IRStatement>(statement));
	}
	void Emit(IRStatement* stmt)
	{
		statements.push_back(stmt);
	}

	std::string GetFreshLabel(const std::string& label)
	{
		return label + std::to_string(label_counter++);
	}
	std::string GetFreshLabel()
	{
		return std::to_string(label_counter++);
	}

	size_t GetID(NodeExprIdent* ident)
	{

		if(ident)
			return ident->symbol->id;

		return 0;
	}
	IRExpr* MakeConst(const ConstValue::Scalar& value) const
	{
		return _allocator.new_object<IRExpr>(IRExprConst{ value });
	}
	void PushLoop(const std::string& start, const std::string& end) {
		continueLabels.push_back(start);
		breakLabels.push_back(end);
	}

	void PopLoop() {
		continueLabels.pop_back();
		breakLabels.pop_back();
	}

	std::vector<std::string> breakLabels;
	std::vector<std::string> continueLabels;
	const NodeProg& ast;
	Mir ir;
	std::vector<IRStatement*> statements;
	std::pmr::polymorphic_allocator<NodeExpr>& _allocator;
	int temp_counter = 0;
	int label_counter = 0;


};