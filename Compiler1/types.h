#pragma once
#include "tokenization.h"
#include <variant>
#include <memory>


struct NodeExpr;

struct NodeExprBinary
{
    Token op;

    std::unique_ptr<NodeExpr> left;
    std::unique_ptr<NodeExpr> right;
};

struct NodeExprUnary
{
    Token op;
    std::unique_ptr<NodeExpr> expr;
};

struct NodeExprCompare
{
    Token op;
    std::unique_ptr<NodeExpr> left;
    std::unique_ptr<NodeExpr> right;
};
struct NodeExprIntLit
{
    Token int_lit;
};
struct NodeExprBoolLit
{
    bool value;
};

struct NodeExprIndent
{
    Token ident;
};

struct NodeExprCall
{
    std::unique_ptr<NodeExpr> expr;
    Token name;
};

struct NodeExpr
{
    std::variant<
        NodeExprIntLit,
        NodeExprIndent,
        NodeExprBinary,
        NodeExprBoolLit,
        NodeExprCompare,
        NodeExprUnary,
        NodeExprCall
    > var;
};


struct NodeStatementExit
{
    std::unique_ptr<NodeExpr> expr;
};


enum class VarType
{
    Int,
    Bool
};

struct NodeStatementVar
{
    VarType type;
    Token ident;
    std::unique_ptr<NodeExpr> expr;
};

struct NodeStatement;
struct NodeFunction;


struct NodeBlock
{
    std::vector<std::unique_ptr<NodeStatement>> statements;

};

struct NodeStatementIf
{
    std::unique_ptr<NodeExpr> condition;
    NodeBlock thenBlock;
    std::optional<NodeBlock> elseBlock;
};

struct NodeStatementWhile
{
    std::unique_ptr<NodeExpr> condition;
    NodeBlock block;
};

struct NodeStatementExpr
{
    std::unique_ptr<NodeExpr> expr;
};
\
struct NodeStatementReturn
{
    std::unique_ptr<NodeExpr> expr;

};


struct NodeStatement
{
    std::variant<
        NodeStatementExit,
        NodeStatementVar,
        NodeBlock,
        NodeStatementIf,
        NodeStatementWhile,
        NodeStatementExpr,
        NodeStatementReturn
    > stat;
};


struct Parameter
{
    VarType type;
    Token ident;
};
struct NodeFunction
{
    Token name;
    std::vector<Parameter> parameter;
    VarType retType;
    NodeBlock body;

    size_t stack_size = 0;
};

struct NodeProg
{
    std::vector<std::unique_ptr<NodeFunction>> functions;
};
