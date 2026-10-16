#pragma once
#include "tokenization.h"
#include <variant>
#include <memory>
#include <assert.h>

struct NodeExpr;
struct Symbol;

struct Declarator;
struct ConstValue {
    using Scalar = std::variant<int, size_t, double, float, bool>;

    // A ConstValue is either a Scalar primitive OR an array/list of ConstValues which are compile time constant
    std::variant<Scalar, std::vector<ConstValue>> data;

    bool is_scalar() const { return std::holds_alternative<Scalar>(data); }
    bool is_array() const { return std::holds_alternative<std::vector<ConstValue>>(data); }

    static ConstValue* get_value_from_array(ConstValue* array, const std::vector<size_t>& indices, size_t start = 0)
    {

        if (array->is_scalar())
        {
            return array;
        }
        else if (array->is_array())
        {
            if (start >= indices.size()) return nullptr;
            auto& main_elem = (*array->get_array())[indices[start]];
            return get_value_from_array(&main_elem, indices, start + 1);
        }

        return nullptr;
    }
    const Scalar& get_scalar() const { return std::get<Scalar>(data); }
    std::vector<ConstValue>* get_array() { return std::get_if<std::vector<ConstValue>>(&data); }
};

//Type information


struct Type {
    enum class Kind { Void, Int, Float, Bool, Pointer, Array, Function };

    Kind kind = Kind::Void;

    std::shared_ptr<Type> element = nullptr;

    size_t array_size = 0;

    bool is_signed = true;

    std::vector<std::shared_ptr<Type>> param_types;
    
    std::string ToString()
    {
        switch (kind)
        {
        case Type::Kind::Int:
            return "int";
            break;
        case Type::Kind::Bool:
            return "bool";
            break;
        case Type::Kind::Float:
            return "float";
            break;
        case Type::Kind::Void:
            return "void";
            break;
        case Type::Kind::Pointer:
            if (element)
                return element->ToString() + "*";

        case Type::Kind::Array:
            if (element)
                return element->ToString() +
                "[" + std::to_string(array_size) + "]";
        }

        return "unknown";
    }

    size_t Size()
    {
        switch (kind)
        {
        case Type::Kind::Int:
            return 4;
        case Type::Kind::Bool:
            return 1;
        case Type::Kind::Float:
            return 4;
        case Type::Kind::Pointer:
            return 8;
        case Type::Kind::Array:
            return array_size * element->Size();
        case Type::Kind::Void:
            return 0;
        default:
            std::cerr << "Unknown type encountered in size calculation." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    static std::shared_ptr<Type> MakeInt() {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Int;
        return t;
    }
    static std::shared_ptr<Type> MakeVoid() {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Void;
        return t;
    }
    static std::shared_ptr<Type> MakePointer(std::shared_ptr<Type> target) {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Pointer;
        t->element = std::move(target);
        return t;
    }

    static std::shared_ptr<Type> MakeArray(std::shared_ptr<Type> elem, size_t size) {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Array;
        t->element = std::move(elem);
        t->array_size = size;
        return t;
    }

    static std::shared_ptr<Type> MakeFloat() {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Float;
        return t;
    }

    static std::shared_ptr<Type> MakeBool() {
        auto t = std::make_shared<Type>();
        t->kind = Kind::Bool;
        return t;
    }
    static std::shared_ptr<Type> GetCommonType(const std::shared_ptr<Type>& lhs,
        const std::shared_ptr<Type>& rhs)
    {
        if (!lhs || !rhs) return nullptr;


        // Floating-point promotion (Float always win)
        if (lhs->kind == Kind::Float || rhs->kind == Kind::Float) {
            if ((lhs->kind == Kind::Int || lhs->kind == Kind::Float) &&
                (rhs->kind == Kind::Int || rhs->kind == Kind::Float)) {
                return MakeFloat();
            }
        }

        // Pointer Arithmetic Rules
        // Pointer + Int -> Pointer
        if (lhs->kind == Kind::Pointer && rhs->kind == Kind::Int) return lhs;
        if (lhs->kind == Kind::Int && rhs->kind == Kind::Pointer) return rhs;

        // Pointer - Pointer -> Int 
        if (lhs->kind == Kind::Pointer && rhs->kind == Kind::Pointer) {
            return MakeInt(); // Or MakePtrDiff()
        }
        //  Array Decay to Pointer Arithmetic
        if (lhs->kind == Kind::Array && rhs->kind == Kind::Int) {
            return MakePointer(lhs->element);
        }
        if (lhs->kind == Kind::Int && rhs->kind == Kind::Array) {
            return MakePointer(rhs->element);
        }
        if (*lhs == *rhs) return lhs;

        // Invalid binary type combination
        return nullptr;
    }

    
    bool operator==(const Type& other) const {
        if (kind != other.kind) return false;
        if (kind == Kind::Array && array_size != other.array_size) return false;

        if (element && other.element) {
            if (!(*element == *other.element)) return false;
        }
        else if (element != other.element) {
            return false;
        }

        if (kind == Kind::Function) {
            if (param_types.size() != other.param_types.size()) return false;
            for (size_t i = 0; i < param_types.size(); ++i) {
                if (!(*param_types[i] == *other.param_types[i])) return false;
            }
        }
        return true;
    }

    bool operator!=(const Type& other) const {
        return !this->operator==(other);
    }
};

struct DeclaratorModifier
{
    enum class Kind
    {
        Pointer,
        Array,
        Function 
    };

    Kind kind;

    // Data payload depending on kind
    size_t array_size = 0;

    // Every modifier wraps an inner modifier
    std::unique_ptr<DeclaratorModifier> child = nullptr;
};

struct Declarator
{
    std::string name;
    std::unique_ptr<DeclaratorModifier> modifier = nullptr;
};

struct ParsedType
{
    std::shared_ptr<Type> type;
    Declarator declarator;
};


struct SourceLocation
{

};
struct Identifier {
    std::string name;
    SourceLocation location; // Tracks line/col for compiler error messages
};
struct NodeExprRef {
    ConstValue* ptr; // Direct pointer to live ConstValue in symbol table
};
struct NodeFunction;

struct NodeExprBinary
{
    TokenType op;

    NodeExpr* left = nullptr;
    NodeExpr* right = nullptr;

};

struct NodeExprUnary
{
    TokenType op;
    NodeExpr* expr = nullptr;

};

struct NodeExprCompare
{
    TokenType op;
    NodeExpr* left = nullptr;
    NodeExpr* right = nullptr;

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
    size_t id = 0;
    std::shared_ptr<Type> type = nullptr;
    std::shared_ptr<Symbol> symbol;

};

struct NodeExprVoid
{

};

struct NodeExprAssignment
{
    NodeExpr* target;
    NodeExpr* expr = nullptr;

};

struct NodeExprCall
{
    NodeFunction* func = nullptr;
    std::vector<NodeExpr*> args;
    std::string name;

};

struct NodeExprInitList {
    std::vector<NodeExpr*> values;
};

struct NodeExprArrayAccess {
    NodeExpr* array;
    std::vector<NodeExpr*> indices;
};


enum class ExprContext {
    RValue,
    LValue
};

typedef std::variant<
    NodeExprIntLit,
    NodeExprIdent, // 1
    NodeExprBinary, // 2
    NodeExprBoolLit, // 3
    NodeExprCompare, // 4
    NodeExprVoid,    // 5
    NodeExprUnary,   // 6
    NodeExprCall,    // 7
    NodeExprAssignment, // 8
    NodeExprInitList, // 9
    NodeExprArrayAccess, // 10
    NodeExprRef
> NodeExpression;
struct NodeExpr
{

    NodeExpression var;
    std::shared_ptr<Type> type = std::make_shared<Type>(Type::Kind::Void);
};


struct NodeStatementExit
{
    NodeExpr* expr = nullptr;
};



struct NodeStatementVar
{
    size_t id = 0;
    std::shared_ptr<Type> element_type = nullptr;
    std::string name;
    std::shared_ptr<Symbol> symbol = nullptr;
    NodeExpr* initializer = nullptr;
};


struct NodeStatement;


struct NodeBlock
{
    std::vector<NodeStatement*> statements;

};

struct NodeStatementIf
{
    NodeExpr* condition = nullptr;
    NodeStatement* thenBody = nullptr;
    NodeStatement* elseBody = nullptr;
};

struct NodeStatementWhile
{
    NodeExpr* condition = nullptr;
    NodeStatement* body = nullptr;
    NodeExpr* update = nullptr;
};

struct NodeStatementExpr
{
    NodeExpr* expr = nullptr;
};
struct NodeStatementReturn
{
    NodeExpr* expr = nullptr;

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
    NodeExpr* target = nullptr;
    NodeExpr* expr = nullptr;

};
struct NodeStatementArray {
    size_t id;
    std::shared_ptr<Type> element_type;
    std::string name;
    NodeExpr* initializer; 
    std::shared_ptr<Symbol> symbol = nullptr;
};

struct NodeStatement
{
    std::variant<
        NodeStatementExit,
        NodeStatementVar, // 1 
        NodeBlock, // 2
        NodeStatementIf, // 3
        NodeStatementWhile, // 4
        NodeStatementBreak, // 5
        NodeStatementContinue, // 6
        NodeStatementExpr, // 7
        NodeStatementReturn, // 8
        NodeStatementAssignment, // 9
        NodeStatementArray // 10
    > stat;
};


struct Parameter
{
    std::string name;                   
    std::shared_ptr<Type> type;         
    size_t id = 0;                      
};
struct NodeFunction
{
    std::string name;
    std::vector<Parameter> parameters;
    std::shared_ptr<Type> retType;
    std::optional<NodeBlock> body;
    size_t stack_size = 0;
};

struct NodeProg
{
    std::vector<NodeFunction> functions;
    NodeBlock global_space;
};


namespace TypeInfo {


    bool SameType(const std::shared_ptr<Type>& a, const std::shared_ptr<Type>& b)
    {
        if (a == b) return true; // Handles both being nullptr or pointing to same address
        if (!a || !b) return false;
        return *a == *b;
    }
    constexpr bool IsArray(const std::shared_ptr<Type>& type) {
        return type->kind == Type::Kind::Array;
    }
    constexpr bool IsArithmetic(const std::shared_ptr<Type>& type) {
        if (!type) return false;
        return type->kind == Type::Kind::Int ||
            type->kind == Type::Kind::Float ||
            type->kind == Type::Kind::Bool;
    }
    constexpr bool IsPointer(const std::shared_ptr<Type>& type) {
        return type->kind == Type::Kind::Pointer; 
    }
    constexpr bool IsVoidPointer(const std::shared_ptr<Type>& type) {
        return (type->kind == Type::Kind::Void) && (type->element) && IsPointer(type->element); 
    }

    bool IsVoid(const std::shared_ptr<Type>& type) {
        auto current = type;

        while (current) {
            if (current->kind == Type::Kind::Void) {
                return true;
            }

            if (current->kind != Type::Kind::Pointer) {
                return false;
            }

            current = current->element;
        }

        return false;
    }

    constexpr bool IsInteger(const std::shared_ptr<Type>& type) {
        return type->kind == Type::Kind::Int; 
    }
    constexpr bool IsScalar(const std::shared_ptr<Type>& type) {
        return (type->kind == Type::Kind::Int) || (type->kind == Type::Kind::Bool) || (type->kind == Type::Kind::Pointer);
    }

    bool CanCompare(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs)
    {
        if (!lhs || !rhs) return false;

        // 1. Identical / structurally equal types (e.g., int vs int, float vs float)
        if (*lhs == *rhs) return true;

        // 2. Both are numeric arithmetic types (e.g., int vs float, char vs int)
        if (IsArithmetic(lhs) && IsArithmetic(rhs)) return true;

        // 3. Pointer comparisons
        if (IsPointer(lhs) && IsPointer(rhs))
        {
            // Any pointer can be compared with void* in C
            if (IsVoidPointer(lhs) || IsVoidPointer(rhs)) return true;

            // Pointers to compatible types (or exact element type match)
            if (lhs->element && rhs->element) {
                return *lhs->element == *rhs->element;
            }
        }

        // 4. Pointer compared against null / integer literal 0
        if ((IsPointer(lhs) && IsInteger(rhs)) || (IsInteger(lhs) && IsPointer(rhs))) {
            return true;
        }

        return false;
    }
    constexpr size_t CalculateSizeInBytes(const Type& type)
    {
        switch (type.kind)
        {
        case Type::Kind::Int:
            return 4;
        case Type::Kind::Bool:
            return 1;
        case Type::Kind::Float:
            return 4;
        case Type::Kind::Pointer:
            return 8; 
        case Type::Kind::Array:
            return type.array_size * CalculateSizeInBytes(*type.element);
        case Type::Kind::Void:
            return 0;
        default:
            std::cerr << "Unknown type encountered in size calculation." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    std::string TypeToString(const std::shared_ptr<Type>& type)
    {
        if (!type) { return "Invalid type"; }
        switch (type->kind)
        {
        case Type::Kind::Int:
            return "int";
            break;
        case Type::Kind::Bool:
            return "bool";
            break;
        case Type::Kind::Float:
            return "float";
            break;
        case Type::Kind::Void:
            return "void";
            break;
        case Type::Kind::Pointer:
            if(type->element)
                return TypeToString(type->element) + "*";

        case Type::Kind::Array:
            if (type->element)
                return TypeToString(type->element) +
                    "[" + std::to_string(type->array_size) + "]";
        }

        return "unknown";
    }

    std::vector<size_t> GetArrayDimensions(const std::shared_ptr<Type>& type)
    {
        std::vector<size_t> dimensions;

        auto current = type;

        while (current )
        {
            if (current->kind == Type::Kind::Array) {
                dimensions.push_back(current->array_size);
            }
            current = current->element;
        }

        return dimensions;
    }

    std::shared_ptr<Type> SetArrayDimensionSize(const std::shared_ptr<Type>& type, size_t dimTarget, size_t newDim)
    {
        if (!type) return nullptr;

        auto root = std::make_shared<Type>(*type);
        auto current = root;

        size_t pos = 0;
        while (pos < dimTarget && current && current->element)
        {
            current->element = std::make_shared<Type>(*current->element);
            current = current->element;
            pos++;
        }

        if (current && pos == dimTarget)
        {
            current->array_size = newDim;
        }

        return root;
    }
    constexpr size_t GetTypeSize(const Type& type) { // TODO: add array
        if (type.kind == Type::Kind::Pointer) {
            return 8; // 64-bit architecture pointer size
        }
        return CalculateSizeInBytes(type);
    }
    std::shared_ptr<Type> GetBaseType(const std::shared_ptr<Type>& type)
    {
        auto current = type;

        auto t = type;
        while (current && (current->kind == Type::Kind::Array))
        {
            current = current->element;
            t = current;
        }
        return t;
    }
    size_t GetTotalArraySize(const std::shared_ptr<Type>& type)
    {
        size_t size = 1;

        auto dimensions = GetArrayDimensions(type);
        for (auto dim : dimensions)
        {
            size *= dim;
        }
        return size * GetTypeSize(*type->element);
    }
  
    const Type& GetPointeeType(const Type& type)
    {
        assert(type.kind == Type::Kind::Pointer);
        return *type.element;
    }
    size_t GetPointerDepth(const std::shared_ptr<Type>& type)
    {
        size_t depth = 0;
        auto current = type;

        while (current && current->kind == Type::Kind::Pointer)
        {
            ++depth;
            current = current->element;
        }

        return depth;
    }
   
    constexpr size_t GetPointeeSize(const Type& type)
    {
        // type must be a pointer
        return GetTypeSize(type);
    }
    std::shared_ptr<Type> MakePointerType(std::shared_ptr<Type>& type)
    {
        Type result;
        result.kind = Type::Kind::Pointer;
        result.element = type;
        return std::make_shared<Type>(result);
    }

    constexpr size_t GetTypeAlignment(const Type& type)
    {
        switch (type.kind)
        {
        case Type::Kind::Int:
            return alignof(int);
        case Type::Kind::Bool:
            return 1;
        case Type::Kind::Float:
            return 4;
        case Type::Kind::Array:
            return 8;
        case Type::Kind::Pointer:
            return 8;
        case Type::Kind::Void:
            return 0;
        default:
            std::cerr << "Unknown type encountered in size calculation." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    //--------------------------------------------------------------------------------------------------------------//
    //                                                                                                              //
    //                                                      Declarator                                              //
    //                                                                                                              //
    //--------------------------------------------------------------------------------------------------------------//

    const std::string& GetDeclaratorName(const Declarator& decl)
    {
        return "";
    }

}
class ValueID
{
public:
    static size_t Create()
    {
        return counter++;
    }

private:
    inline static size_t counter = 0;
};