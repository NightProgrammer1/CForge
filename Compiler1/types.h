#pragma once
#include "tokenization.h"
#include <variant>
#include <memory>


struct NodeExpr;

struct NodeExprBinary
{
    Token op;

    NodeExpr* left;
    NodeExpr* right;
};

struct NodeExprUnary
{
    Token op;
    NodeExpr* expr;
};

struct NodeExprCompare
{
    Token op;
    NodeExpr* left;
    NodeExpr* right;
};
struct NodeExprIntLit
{
    int value;
};
struct NodeExprBoolLit
{
    bool value;
};

struct NodeExprIndent
{
    std::string name;
};

struct NodeExprVoid
{

};

struct NodeExprCall
{
    std::vector<NodeExpr*> args;
    std::string name;
};

struct NodeExpr
{
    std::variant<
        NodeExprIntLit,
        NodeExprIndent,
        NodeExprBinary,
        NodeExprBoolLit,
        NodeExprCompare,
        NodeExprVoid,
        NodeExprUnary,
        NodeExprCall
    > var;
};


struct NodeStatementExit
{
    NodeExpr* expr;
};


enum class Type {
    Int,
    Bool,
    Void
};

struct NodeStatementVar
{
    std::string name;
    NodeExpr* expr;
    Type type;
};

struct NodeStatement;


struct NodeBlock
{
    std::vector<NodeStatement*> statements;

};

struct NodeStatementIf
{
    NodeExpr* condition;
    NodeBlock thenBlock;
    std::optional<NodeBlock> elseBlock;
};

struct NodeStatementWhile
{
    NodeExpr* condition;
    NodeBlock block;
};

struct NodeStatementExpr
{
    NodeExpr* expr;
};
\
struct NodeStatementReturn
{
    NodeExpr* expr;

};
struct NodeStatementAssignment
{
    Token ident;     
    NodeExpr* expr;

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
        NodeStatementReturn,
        NodeStatementAssignment
    > stat;
};


struct Parameter
{
    Type type;
    std::string name;
};
struct NodeFunction
{
    std::string name;
    std::vector<Parameter> parameters;
    Type retType;
    std::optional<NodeBlock> body;
    size_t stack_size = 0;
};

struct NodeProg
{
    std::vector<NodeFunction> functions;
};
