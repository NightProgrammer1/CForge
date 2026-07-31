#pragma once
#include "tokenization.h"
#include <variant>
#include <memory>

struct NodeFunction;
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

struct NodeExprIdent
{
    std::string name;
};

struct NodeExprVoid
{

};

struct NodeExprAssignment
{
    std::string name;
    NodeExpr* expr;
};

struct NodeExprCall
{
    NodeFunction* func;
    std::vector<NodeExpr*> args;
    std::string name;
};

struct NodeExpr
{
    std::variant<
        NodeExprIntLit,
        NodeExprIdent, // 1
        NodeExprBinary, // 2
        NodeExprBoolLit, // 3
        NodeExprCompare, // 4
        NodeExprVoid,    // 5
        NodeExprUnary,   // 6
        NodeExprCall,    // 7
        NodeExprAssignment // 8
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
struct NodeStatementFor
{
    std::variant< 
        NodeExpr*,
        NodeStatement*>
        init;

    NodeExpr* condition;
    NodeExpr* update;
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
struct NodeStatementBreak
{

};

struct NodeStatementContinue
{

};
struct NodeStatementAssignment
{
    std::string name;     
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
        NodeStatementBreak,
        NodeStatementContinue,
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
