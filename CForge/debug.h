#include "fmt/format.h"
#include <chrono>

class ScopedTimer {
public:
	ScopedTimer(const std::string& name, bool enabled)
		: name_(name), enabled_(enabled), start_(std::chrono::high_resolution_clock::now()) {
	}

	~ScopedTimer() {
		if (!enabled_) return;

		auto end = std::chrono::high_resolution_clock::now();
		auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
		double duration_ms = static_cast<double>(duration_us) / 1000.0;

		
		fmt::print("[Profile] {}: {:.3f} ms ({} µs)\n", name_, duration_ms, duration_us);
	}

private:
	std::string name_;
	bool enabled_;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};



//                                                                                                                                            //
//                                                                                                                                            //
//                                                                                                                                            //
//  -------------------------------------------------------------------PRINTER--------------------------------------------------------------  //
//                                                                                                                                            //
//                                                                                                                                            //
//                                                                                                                                            //

void PrintIndent(int indent) {
    for (int i = 0; i < indent; ++i) {
        fmt::print("  ");
    }
}
// --- ANSI Color Constants ---
constexpr const char* COL_RESET = "\033[0m";
constexpr const char* COL_TREE = "\033[90m"; 
constexpr const char* COL_EXPR = "\x1b[38;5;14m"; 
constexpr const char* COL_STMT = "\x1b[38;5;9m"; 
constexpr const char* COL_LIT = "\033[32m"; 
constexpr const char* COL_OP = "\033[33m"; 
constexpr const char* COL_TYPE = "\033[35m"; 

void PrintBranch(const std::string& prefix, bool is_last, const std::string& text) {
    std::string branch = is_last ? "└── " : "├── ";
    fmt::print("{}{}{}{}{}\n", COL_TREE, prefix, branch, COL_RESET, text);
}

void PrintNode(const NodeExpr* expr, const std::string& prefix, bool is_last);
void PrintNode(const NodeExpr* expr, const std::string& prefix, bool is_last) {
    if (!expr) return;

    std::string type_suffix = "";
    if (expr->type) {
        type_suffix = fmt::format(" {}[{}{}{}]", COL_TYPE, COL_LIT, expr->type.ToString(), COL_RESET);
    }

    struct Visitor {
        std::string prefix;
        bool is_last;
        std::string type_suffix;

		void operator()(const NodeExprLiteral& lit) const {
			std::string val_str = std::visit([](auto&& v) {
				return fmt::format("{}", v);
				}, lit.value);

			std::string text = fmt::format("{}Literal{}: {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, val_str, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}
        void operator()(const NodeExprIdent& ident) const {
            std::string text = fmt::format("{}Identifier{}({}{}{}){}", COL_EXPR, COL_RESET, COL_LIT, ident.name, COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);
        }
        void operator()(const NodeExprBinary& bin) const {
            std::string text = fmt::format("{}BinaryExpr{} ({}{}{}){}", COL_EXPR, COL_RESET, COL_OP, Tokenizer::ToString(bin.op), COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);

            std::string next_prefix = prefix + (is_last ? "     " : "│    ");
            PrintNode(bin.left, next_prefix, false);
            PrintNode(bin.right, next_prefix, true);
        }
        void operator()(const NodeExprCompare& comp) const {
            std::string text = fmt::format("{}CompareExpr{} ({}{}{}){}", COL_EXPR, COL_RESET, COL_OP, Tokenizer::ToString(comp.op), COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);

            std::string next_prefix = prefix + (is_last ? "     " : "│    ");
            PrintNode(comp.left, next_prefix, false);
            PrintNode(comp.right, next_prefix, true);
        }
        void operator()(const NodeExprVoid&) const {
            PrintBranch(prefix, is_last, fmt::format("{}VoidExpr{}{}", COL_EXPR, COL_RESET, type_suffix));
        }
        void operator()(const NodeExprUnary& unary) const {
            std::string text = fmt::format("{}UnaryExpr{} ({}{}{}){}", COL_EXPR, COL_RESET, COL_OP, Tokenizer::ToString(unary.op), COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);

            std::string next_prefix = prefix + (is_last ? "     " : "│    ");
            PrintNode(unary.expr, next_prefix, true);
        }
        void operator()(const NodeExprCall& call) const {
            std::string text = fmt::format("{}CallExpr{}{}", COL_EXPR, COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);

            std::string next_prefix = prefix + (is_last ? "     " : "│    ");
            for (size_t i = 0; i < call.args.size(); ++i) {
                PrintNode(call.args[i], next_prefix, i == call.args.size() - 1);
            }
        }
        void operator()(const NodeExprAssignment& assign) const {
            PrintBranch(prefix, is_last, fmt::format("{}AssignmentExpr{}{}", COL_EXPR, COL_RESET, type_suffix));
            std::string next_prefix = prefix + (is_last ? "     " : "│    ");

            PrintNode(assign.target, next_prefix, false);
            PrintNode(assign.expr, next_prefix, true);
        }
        void operator()(const NodeExprInitList& list) const {
            PrintBranch(prefix, is_last, fmt::format("{}InitListExpr{}{}", COL_EXPR, COL_RESET, type_suffix));
            std::string next_prefix = prefix + (is_last ? "     " : "│    ");

            for (size_t i = 0; i < list.values.size(); i++) {
                PrintNode(list.values[i], next_prefix, i == list.values.size() - 1);
            }
        }
        void operator()(const NodeExprArrayAccess& access) const {
            std::string array_name = "";
            if (auto name = std::get_if<NodeExprIdent>(&access.array->var)) {
                array_name = name->name;
            }

            std::string text = fmt::format("{}ArrayAccessExpr{}({}{}{}){}", COL_EXPR, COL_RESET, COL_LIT, array_name, COL_RESET, type_suffix);
            PrintBranch(prefix, is_last, text);

            std::string next_prefix = prefix + (is_last ? "     " : "│    ");
            for (size_t i = 0; i < access.indices.size(); i++) {
                PrintNode(access.indices[i], next_prefix, i == access.indices.size() - 1);
            }
        }
        
    } visitor{ prefix, is_last, type_suffix };

    std::visit(visitor, expr->var);
}
void PrintFunction(const NodeFunction& func, bool is_last_func)
{
	std::string func_text = fmt::format("{}FunctionDecl{}: {}{} (returns: {}{}{})",
		COL_RESET, COL_RESET, COL_LIT, func.name, COL_TYPE, TypeInfo::TypeToString(func.retType), COL_RESET);

	PrintBranch("", is_last_func, func_text);

	std::string func_prefix = is_last_func ? "     " : "│    ";
	const auto& statements = func.body->statements;

	for (size_t j = 0; j < statements.size(); ++j) {
		bool is_last_stmt = (j == statements.size() - 1);

		struct StmtVisitor {
			std::string prefix;
			bool is_last;

			void operator()(const NodeStatementExit&) const {
				PrintBranch(prefix, is_last, fmt::format("{}StatementExit{}", COL_STMT, COL_RESET));
			}
			void operator()(const NodeStatementVar& var) const {
				std::string type_str = "";
				std::string size_str = "";

				if (var.type) {
					type_str = fmt::format(" elem: {}{}{}", COL_TYPE, var.type.ToString(), COL_RESET);

					auto baseType = TypeInfo::GetBaseType(var.type);
					if (baseType && baseType.Size() > 0) {
						size_str = fmt::format(" size: {}{}{}", COL_LIT, var.type.Size(), COL_RESET);
					}
				}

				// Dynamically label as array or scalar variable
				std::string label = TypeInfo::IsArray(var.type) ? "StatementArray" : "StatementVar";

				std::string text = fmt::format("{}{}{}: {}{}{}{}{}",
					COL_STMT, label, COL_RESET,
					COL_LIT, var.name, COL_RESET,
					type_str, size_str);

				PrintBranch(prefix, is_last, text);

				if (var.initializer) {
					std::string next_prefix = prefix + (is_last ? "     " : "│    ");
					PrintNode(var.initializer, next_prefix, true);
				}
			}
			void operator()(const NodeBlock& block) const {
				PrintBranch(prefix, is_last, fmt::format("{}CompoundStmt{}", COL_STMT, COL_RESET));
				for (size_t k = 0; k < block.statements.size(); ++k) {
					std::string next_prefix = prefix + (is_last ? "     " : "│    ");
					std::visit(StmtVisitor{ next_prefix, k == block.statements.size() - 1 }, block.statements[k]->stat);
				}
			}
			void operator()(const NodeStatementIf& _if) const {
				PrintBranch(prefix, is_last, fmt::format("{}IfStmt{}", COL_STMT, COL_RESET));
				std::string next_prefix = prefix + (is_last ? "     " : "│    ");

				// Print condition first
				PrintNode(_if.condition, next_prefix, _if.thenBody == nullptr && _if.elseBody == nullptr);

				if (_if.thenBody) {
					std::visit(StmtVisitor{ next_prefix, _if.elseBody == nullptr }, _if.thenBody->stat);
				}
				if (_if.elseBody) {
					std::visit(StmtVisitor{ next_prefix, true }, _if.elseBody->stat);
				}
			}
			void operator()(const NodeStatementWhile& _while) const {
				PrintBranch(prefix, is_last, fmt::format("{}WhileStmt{}", COL_STMT, COL_RESET));
				std::string next_prefix = prefix + (is_last ? "     " : "│    ");

				PrintNode(_while.condition, next_prefix, false);

				if (_while.body) {
					std::visit(StmtVisitor{ next_prefix, _while.update == nullptr }, _while.body->stat);
				}
				if (_while.update) {
					PrintNode(_while.update, next_prefix, true);
				}
			}
			void operator()(const NodeStatementBreak&) const {
				PrintBranch(prefix, is_last, fmt::format("{}StatementBreak{}", COL_STMT, COL_RESET));
			}
			void operator()(const NodeStatementContinue&) const {
				PrintBranch(prefix, is_last, fmt::format("{}StatementContinue{}", COL_STMT, COL_RESET));
			}
			void operator()(const NodeStatementExpr& expr) const {
				PrintBranch(prefix, is_last, fmt::format("{}StatementExpr{}", COL_STMT, COL_RESET));
				std::string next_prefix = prefix + (is_last ? "     " : "│    ");
				PrintNode(expr.expr, next_prefix, true);
			}
			void operator()(const NodeStatementReturn& ret) const {
				PrintBranch(prefix, is_last, fmt::format("{}ReturnStmt{}", COL_STMT, COL_RESET));
				if (ret.expr) {
					std::string next_prefix = prefix + (is_last ? "     " : "│    ");
					PrintNode(ret.expr, next_prefix, true);
				}
			}
			void operator()(const NodeStatementAssignment& assign) const {
				PrintBranch(prefix, is_last, fmt::format("{}StatementAssignment{}", COL_STMT, COL_RESET));
				std::string next_prefix = prefix + (is_last ? "     " : "│    ");

				PrintNode(assign.target, next_prefix, false);
				PrintNode(assign.expr, next_prefix, true);
			}
		} stmt_visitor{ func_prefix, is_last_stmt };

		std::visit(stmt_visitor, statements[j]->stat);
	}
}

void PrintAST(const NodeProg& prog) {
	fmt::print("{}[TranslationUnit]{}\n", COL_EXPR, COL_RESET);

	std::string base_prefix = "";
	std::string child_prefix = base_prefix + "    ";

	for (size_t i = 0; i < prog.items.size(); ++i) {
		bool last_item = (i == prog.items.size() - 1);
		const auto& item = prog.items[i];

		bool print_global_space = false;
		if (std::holds_alternative<NodeStatementVar*>(item.decl)) {
			if (i == 0 || std::holds_alternative<NodeFunction*>(prog.items[i - 1].decl)) {
				print_global_space = true;
			}
		}

		if (print_global_space) {
			bool global_space_is_last = true;
			for (size_t j = i + 1; j < prog.items.size(); ++j) {
				if (std::holds_alternative<NodeFunction*>(prog.items[j].decl)) {
					global_space_is_last = false;
					break;
				}
			}
			PrintBranch(base_prefix, global_space_is_last, fmt::format("{0}GlobalSpace{0}", COL_RESET));
		}

		std::visit([&](auto* node) {
			using T = std::decay_t<decltype(*node)>;

			if constexpr (std::is_same_v<T, NodeFunction>) {
				bool is_last_func = true;
				for (size_t j = i + 1; j < prog.items.size(); ++j) {
					if (std::holds_alternative<NodeFunction*>(prog.items[j].decl)) {
						is_last_func = false;
						break;
					}
				}
				PrintFunction(*node, is_last_func);
			}
			else if constexpr (std::is_same_v<T, NodeStatementVar>) {
				bool is_last_var = last_item;
				if (!last_item && std::holds_alternative<NodeFunction*>(prog.items[i + 1].decl)) {
					is_last_var = true;
				}

				std::string type_str = "";
				std::string size_str = "";

				if (node->type) {
					type_str = fmt::format(" elem: {}{}{}", COL_TYPE, node->type.ToString(), COL_RESET);

					auto baseType = TypeInfo::GetBaseType(node->type);
					if (baseType && baseType.Size() > 0) {
						size_str = fmt::format(" size: {}{}{}", COL_LIT, baseType.Size(), COL_RESET);
					}
				}

				std::string label = TypeInfo::IsArray(node->type) ? "StatementArray" : "StatementVar";

				std::string text = fmt::format("{}{}{}: {}{}{}{}{}",
					COL_STMT, label, COL_RESET,
					COL_LIT, node->name, COL_RESET,
					type_str, size_str);

				PrintBranch(child_prefix, is_last_var, text);

				if (node->initializer) {
					std::string next_prefix = child_prefix + (is_last_var ? "     " : "│    ");
					PrintNode(node->initializer, next_prefix, true);
				}
			}
			}, item.decl);
	}

	
}


void PrintIRNode(const IRExpr* expr, const std::string& prefix, bool is_last) {
	if (!expr) return;
	std::string type_suffix = "";
	if (expr->type) {
		type_suffix = fmt::format(" {}[{}{}{}]", COL_TYPE, COL_LIT, expr->type.ToString(), COL_RESET);
	}
	struct Visitor {
		std::string prefix;
		bool is_last;
		std::string type_suffix;

		void operator()(const IRExprConst& c) const {
			std::string val_str = std::visit([](auto&& v) {
				return fmt::format("{}", v);
				}, c.data);
			std::string text = fmt::format("{}IRExprConst{}: {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, val_str, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const IRExprName& n) const {
			std::string text = fmt::format("{}IRExprName{}: {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, n.name, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const IRExprTemp& t) const {
			std::string text = fmt::format("{}IRExprTemp{}: t{}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, t.virtualRegister.id, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const IRExprStackObject& s) const {
			std::string text = fmt::format("{}IRExprStackObject{}: offset {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, s.id.offset, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const IRExprBinop& binop) const {
			std::string op_str = "";
			std::string text = fmt::format("{}IRExprBinop{} ({}{}{}){}", COL_EXPR, COL_RESET, COL_OP, op_str, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(binop.left, next_prefix, false);
			PrintIRNode(binop.right, next_prefix, true);
		}
		void operator()(const IRExprMem& mem) const {
			std::string text = fmt::format("{}IRExprMem{}{}", COL_EXPR, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(mem.address, next_prefix, true);
		}
		void operator()(const IRExprDereference& deref) const {
			std::string text = fmt::format("{}IRExprDereference{}{}", COL_EXPR, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(deref.address, next_prefix, true);
		}
		void operator()(const IRExprCall& call) const {
			std::string text = fmt::format("{}IRExprCall{}{}", COL_EXPR, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(call.func, next_prefix, call.args.empty());
			for (size_t i = 0; i < call.args.size(); ++i) {
				PrintIRNode(call.args[i], next_prefix, i == call.args.size() - 1);
			}
		}
		void operator()(const IRExprESEQ& eseq) const {
			std::string text = fmt::format("{}IRExprESEQ{}{}", COL_EXPR, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(eseq.expr, next_prefix, true);
		}
		void operator()(const IRExprAddressOf& addr) const {
			std::string text = fmt::format("{}IRExprAddressOf{}{}", COL_EXPR, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(addr.object, next_prefix, true);
		}
		void operator()(const IRExprGlobal& global) const {
			std::string text = fmt::format("{}IRExprGlobal{}: {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, global.name, COL_RESET, type_suffix);
			PrintBranch(prefix, is_last, text);
		}

	} visitor{ prefix, is_last, type_suffix };

	std::visit(visitor, expr->expr);
}

void PrintInstruction(const Instruction& ins, const std::string& prefix, bool is_last) {
	struct Visitor {
		std::string prefix;
		bool is_last;
		void operator()(const TACBinop& binop) const {
			std::string op_str = BinaryOpToString(binop.op);
			std::string text = fmt::format("{}TACBinop{} ({}{}{})", COL_STMT, COL_RESET, COL_OP, op_str, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dest:{}", COL_TYPE, COL_RESET));
			std::string dest_prefix = next_prefix + "    ";
			PrintIRNode(binop.dst, dest_prefix, true);

			PrintBranch(next_prefix, false, fmt::format("{}left:{}", COL_TYPE, COL_RESET));
			std::string left_prefix = next_prefix + "    ";
			PrintIRNode(binop.left, left_prefix, true);

			PrintBranch(next_prefix, true, fmt::format("{}right:{}", COL_TYPE, COL_RESET));
			std::string right_prefix = next_prefix + "    ";
			PrintIRNode(binop.right, right_prefix, true);
		}
		void operator()(const TACMove& move) const {
			std::string text = fmt::format("{}TACMove{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dst:{}", COL_TYPE, COL_RESET));
			std::string dst_prefix = next_prefix + "    ";
			PrintIRNode(move.dst, dst_prefix, true);

			PrintBranch(next_prefix, true, fmt::format("{}src:{}", COL_TYPE, COL_RESET));
			std::string src_prefix = next_prefix + "    ";
			PrintIRNode(move.src, src_prefix, true);
		}
		void operator()(const TACMoveMem& moveMem) const {
			std::string text = fmt::format("{}TACMoveMem{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dst:{}", COL_TYPE, COL_RESET));
			std::string dst_prefix = next_prefix + "    ";
			PrintIRNode(moveMem.dst, dst_prefix, true);

			PrintBranch(next_prefix, true, fmt::format("{}src:{}", COL_TYPE, COL_RESET));
			std::string src_prefix = next_prefix + "    ";
			PrintIRNode(moveMem.src, src_prefix, true);
		}
		void operator()(const TACLoad& load) const {
			std::string text = fmt::format("{}TACLoad{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dst:{}", COL_TYPE, COL_RESET));
			std::string dst_prefix = next_prefix + "    ";
			PrintIRNode(load.dst, dst_prefix, true);

			PrintBranch(next_prefix, true, fmt::format("{}src:{}", COL_TYPE, COL_RESET));
			std::string src_prefix = next_prefix + "    ";
			PrintIRNode(load.src, src_prefix, true);
		}
		void operator()(const TACAddressOf& addr) const {
			std::string text = fmt::format("{}TACAddressOf{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dst:{}", COL_TYPE, COL_RESET));
			std::string dst_prefix = next_prefix + "    ";
			PrintIRNode(addr.dst, dst_prefix, true);

			PrintBranch(next_prefix, true, fmt::format("{}object:{}", COL_TYPE, COL_RESET));
			std::string obj_prefix = next_prefix + "    ";
			PrintIRNode(addr.object, obj_prefix, true);
		}
		void operator()(const TACRelationalCJump& relCJump) const {
			std::string op_str = RelationalOpToString(relCJump.op);
			std::string text = fmt::format("{}TACRelationalCJump{} ({}{}{})", COL_STMT, COL_RESET, COL_OP, op_str, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(relCJump.left, next_prefix, false);
			PrintIRNode(relCJump.right, next_prefix, false);
			PrintBranch(next_prefix, true, fmt::format("false: {}", relCJump.labelFalse));
		}
		void operator()(const TACCJump& cjump) const {
			std::string text = fmt::format("{}TACCJump{} (target: {}{}{})", COL_STMT, COL_RESET, COL_LIT, cjump.labelFalse, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			PrintIRNode(cjump.cond, next_prefix, true);
		}
		void operator()(const TACJump& jump) const {
			std::string text = fmt::format("{}TACJump{} (target: {}{}{})", COL_STMT, COL_RESET, COL_LIT, jump.label, COL_RESET);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const TACLabel& label) const {
			std::string text = fmt::format("{}TACLabel{}: {}{}{}", COL_STMT, COL_RESET, COL_LIT, label.label, COL_RESET);
			PrintBranch(prefix, is_last, text);
		}
		void operator()(const TACCall& call) const {
			std::string text = fmt::format("{}TACCall{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);

			std::string next_prefix = prefix + (is_last ? "     " : "│    ");
			if (call.dst) {
				PrintIRNode(call.dst, next_prefix, false);
			}
			PrintIRNode(call.dst, next_prefix, call.args.empty());
			for (size_t i = 0; i < call.args.size(); ++i) {
				PrintIRNode(call.args[i], next_prefix, i == call.args.size() - 1);
			}
		}
		void operator()(const TACReturn& ret) const {
			std::string text = fmt::format("{}TACReturn{}", COL_STMT, COL_RESET);
			PrintBranch(prefix, is_last, text);
			std::string next_prefix = prefix + (is_last ? "     " : "│    ");

			PrintBranch(next_prefix, false, fmt::format("{}dst:{}", COL_TYPE, COL_RESET));
			std::string dst_prefix = next_prefix + "    ";
			if (ret.value) {
				PrintIRNode(ret.value, dst_prefix, true);
			}
		}
	} visitor{ prefix, is_last };

	std::visit(visitor, ins.expr);
}

void PrintFunctionTAC(const TAC& tac, bool is_last_func) {
	std::string func_text = fmt::format("{}FunctionTAC{}: {}{}",
		COL_RESET, COL_RESET, COL_LIT, tac.name);

	PrintBranch("", is_last_func, func_text);

	std::string func_prefix = is_last_func ? "     " : "│    ";
	const auto& instructions = tac.instructions;

	for (size_t j = 0; j < instructions.size(); ++j) {
		bool is_last_inst = (j == instructions.size() - 1);
		PrintInstruction(*instructions[j], func_prefix, is_last_inst);
	}
}

void PrintProgramTAC(const TACProgram& program) {
	fmt::print("{}[Three Address Code (TAC)]{}\n", COL_EXPR, COL_RESET);
	for (size_t i = 0; i < program.functions.size(); ++i) {
		bool last_func = (i == program.functions.size() - 1);
		PrintFunctionTAC(program.functions[i], last_func);
	}
}