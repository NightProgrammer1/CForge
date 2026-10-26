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
constexpr const char* COL_EXPR = "\x1b[38;5;153m"; 
constexpr const char* COL_STMT = "\x1b[38;5;223m"; 
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
        type_suffix = fmt::format(" {}[{}{}{}]", COL_TYPE, COL_LIT, expr->type->ToString(), COL_RESET);
    }

    struct Visitor {
        std::string prefix;
        bool is_last;
        std::string type_suffix;

        void operator()(const NodeExprIntLit& lit) const {
            std::string text = fmt::format("{}IntLit{}: {}{}{}{}", COL_EXPR, COL_RESET, COL_LIT, lit.value, COL_RESET, type_suffix);
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
        void operator()(const NodeExprBoolLit&) const {
            PrintBranch(prefix, is_last, fmt::format("{}BoolLit{}{}", COL_EXPR, COL_RESET, type_suffix));
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
void PrintAST(const NodeProg& prog) {
    fmt::print("{}[TranslationUnit]{}\n", COL_EXPR, COL_RESET);

    const auto& functions = prog.functions;
    for (size_t i = 0; i < functions.size(); ++i) {
        bool is_last_func = (i == functions.size() - 1);
        const auto& func = functions[i];

        std::string func_text = fmt::format("{}FunctionDecl{}: {}{} (returns: {}{}{})",
            COL_EXPR, COL_RESET, COL_LIT, func.name, COL_TYPE, TypeInfo::TypeToString(func.retType), COL_RESET);

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
                    std::string text = fmt::format("{}VarDecl{}: {}{}{}", COL_STMT, COL_RESET, COL_LIT, var.name, COL_RESET);
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
                void operator()(const NodeStatementArray& array) const {
                    std::string type_str = "";
                    std::string size_str = "";

                    if (array.element_type) {
                        type_str = fmt::format(" elem: {}{}{}", COL_TYPE, array.element_type->ToString(), COL_RESET);
                        size_str = fmt::format(" size: {}{}{}", COL_LIT, array.element_type->Size() / TypeInfo::GetBaseType(array.element_type)->Size(), COL_RESET);

                    }
                    std::string text = fmt::format("{}StatementArray{}: {}{}{}{}{}",
                        COL_STMT, COL_RESET,
                        COL_LIT, array.name, COL_RESET,
                        type_str, size_str);

                    PrintBranch(prefix, is_last, text);

                    if (array.initializer) {
                        std::string next_prefix = prefix + (is_last ? "     " : "│    ");
                        PrintNode(array.initializer, next_prefix, true);
                    }
                }
            } stmt_visitor{ func_prefix, is_last_stmt };

            std::visit(stmt_visitor, statements[j]->stat);
        }
    }
}
