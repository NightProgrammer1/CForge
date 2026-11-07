#pragma once
#include "tokenization.h"
#include <variant>
#include <memory>
#include <assert.h>
#include <optional>
#include <vector>
#include <string>
#include <iostream>
#include <cassert>


struct NodeExpr;
struct Symbol;

struct Declarator;
struct ConstValue {


	
    inline static size_t copy_count = 0;
	struct Pointer {
		size_t variable_id;
		std::vector<size_t> indices; // Tracks paths
		int64_t element_offset = 0;
	};
	using Scalar = std::variant<int, size_t, double, float, bool>;

	// A ConstValue is either a Scalar primitive OR an array/list of ConstValues which are compile time constant
	std::variant<Scalar, std::vector<ConstValue>, Pointer> data;

	ConstValue() = default;

	ConstValue(const ConstValue& other) : data(other.data) {
		copy_count++;
		 //std::cout << "[Tracking] ConstValue copied! Total copies: " << copy_count << "\n";
	}

    ConstValue& operator=(ConstValue&& other) {
        if (this != &other) {
			data = std::move(other.data);
        }

        return *this;
    }
	ConstValue& operator=(const ConstValue& other) {
		if (this != &other) {
			data = other.data;
			copy_count++;
			// std::cout << "[Tracking] ConstValue copy assigned! Total copies: " << copy_count << "\n";
		}
		return *this;
	}

	template <typename T, typename = std::enable_if_t<
		!std::is_same_v<std::decay_t<T>, ConstValue>
		>>
		ConstValue(T&& val) : data(std::forward<T>(val)) {}

    bool is_scalar() const { return std::holds_alternative<Scalar>(data); }
    bool is_array() const { return std::holds_alternative<std::vector<ConstValue>>(data); }

	void set_value_at_indices(const std::vector<size_t>& indices, const ConstValue& value, size_t start = 0)
	{
		set_value_at_indices_impl(indices, value, start);
	}

	void set_value_at_indices(const std::vector<size_t>& indices, ConstValue&& value, size_t start = 0)
	{
		set_value_at_indices_impl(indices, std::move(value), start);
	}
    static ConstValue* get_value_from_array(ConstValue* array, const std::vector<size_t>& indices, size_t start = 0)
    {

        if (array->is_scalar())
        {
            return array;
        }
        else if (array->is_pointer())
        {
            return array;
        }
        else if (array->is_array())
        {

            if (start >= indices.size() || indices[start] >= array->size()) {
				std::cerr << "Compile-Time Error: Array index " << indices[start]
					<< " is out of bounds for array of size " << array->size() << "\n";
				std::exit(EXIT_FAILURE);
            }
            auto& main_elem = (*array->get_array())[indices[start]];
            return get_value_from_array(&main_elem, indices, start + 1);
        }

        return nullptr;
    }
    //Creates a default array of elemCount 
    static ConstValue create_array(size_t elemCount)
    {
        return ConstValue{ std::vector<ConstValue>(elemCount) };
    }
	static ConstValue create_array(const std::vector<size_t>& dims, size_t depth = 0)
	{
		if (depth == dims.size() - 1) {
			return ConstValue::create_array(dims[depth]);
		}
		std::vector<ConstValue> innerElements;
		innerElements.reserve(dims[depth]);
		for (size_t i = 0; i < dims[depth]; ++i) {
			innerElements.push_back(create_array(dims, depth + 1));
		}
		return ConstValue{ std::move(innerElements) };
	}
    const Scalar& get_scalar() const { return std::get<Scalar>(data); }
    std::vector<ConstValue>* get_array() { return std::get_if<std::vector<ConstValue>>(&data); }

    size_t size() {
        if (is_array()) {
            return get_array()->size();
        }

        return 1;
    }
    bool is_pointer() const { return std::holds_alternative<Pointer>(data); }

    const Pointer* get_pointer() const { return std::get_if<Pointer>(&data); }
    Pointer* get_pointer() { return std::get_if<Pointer>(&data); }

private:
	template <typename T>
    void set_value_at_indices_impl(const std::vector<size_t>& indices, T&& value, size_t start)
    {
        auto arr = get_array();
        if (!arr) return;
        if (start >= indices.size()) return;

        size_t idx = indices[start];
        if (idx >= arr->size()) return;

        if (start == indices.size() - 1) {
            (*arr)[idx] = std::forward<T>(value);
        }
        else {
            (*arr)[idx].set_value_at_indices(indices, std::forward<T>(value), start + 1);
        }
    }
};

//Type information


struct Type;

struct Modifier
{
	bool is_const = false;
	bool is_volatile = false;
};
struct QualifiedType {
	std::shared_ptr<Type> base_type;
	Modifier mod;

	std::string ToString() const;
	size_t Size() const;

	bool operator==(const QualifiedType& other) const;

	bool operator!=(const QualifiedType& other) const {
		return !(*this == other);
	}
	operator bool() const {
		return base_type != nullptr;
	}

	static QualifiedType MakeInt();
	static QualifiedType MakeVoid();
	static QualifiedType MakePointer(const QualifiedType& target, const Modifier& mod);
	static QualifiedType MakeArray(const QualifiedType& elem, size_t size);
	static QualifiedType MakeFloat();
	static QualifiedType MakeBool();
	static QualifiedType GetCommonType(const QualifiedType& lhs, const QualifiedType& rhs);
};

struct Type {
	enum class Kind { Void, Int, Float, Bool, Pointer, Array, Function };

	Kind kind = Kind::Void;

	QualifiedType element; 

	size_t array_size = 0;

	bool is_signed = true;

	std::vector<QualifiedType> param_types;

	std::string ToString() const;

	size_t Size() const
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
			return array_size * element.Size();
		case Type::Kind::Void:
			return 0;
		default:
			std::cerr << "Unknown base_type encountered in size calculation." << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	bool operator==(const Type& other) const {
		if (kind != other.kind) return false;
		if (kind == Kind::Array && array_size != other.array_size) return false;

		if (element != other.element) {
			return false;
		}

		if (kind == Kind::Function) {
			if (param_types.size() != other.param_types.size()) return false;
			for (size_t i = 0; i < param_types.size(); ++i) {
				if (param_types[i] != other.param_types[i]) return false;
			}
		}
		return true;
	}

	bool operator!=(const Type& other) const {
		return !(*this == other);
	}
};

// --- Out-of-line method implementations  ---
inline bool QualifiedType::operator==(const QualifiedType& other) const {
	//if (is_const != other.is_const || is_volatile != other.is_volatile) return false;
	if (base_type == other.base_type) return true;
	if (!base_type || !other.base_type) return false;
	return *base_type == *other.base_type; 
}
inline std::string QualifiedType::ToString() const {
	if (!base_type) return "invalid";
	std::string prefix;
	if (mod.is_const) prefix += "const ";
	if (mod.is_volatile) prefix += "volatile ";
	return prefix + base_type->ToString();
}

inline size_t QualifiedType::Size() const {
	if (!base_type) return 0;
	return base_type->Size();
}

inline std::string Type::ToString() const {
	switch (kind)
	{
	case Type::Kind::Int:
		return "int";
	case Type::Kind::Bool:
		return "bool";
	case Type::Kind::Float:
		return "float";
	case Type::Kind::Void:
		return "void";
	case Type::Kind::Pointer:
		if (element.base_type)
			return element.ToString() + "*";
		return "void*";
	case Type::Kind::Array:
		if (element.base_type)
			return element.ToString() + "[" + std::to_string(array_size) + "]";
	}
	return "unknown";
}

inline QualifiedType QualifiedType::MakeInt() {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Int;
	return QualifiedType{ t, false, false };
}

inline QualifiedType QualifiedType::MakeVoid() {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Void;
	return QualifiedType{ t, false, false };
}

inline QualifiedType QualifiedType::MakePointer(const QualifiedType& target, const Modifier& mod = {}) {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Pointer;
	t->element = target;
	return QualifiedType{ t, mod.is_const, mod.is_volatile };
}

inline QualifiedType QualifiedType::MakeArray(const QualifiedType& elem, size_t size) {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Array;
	t->element = elem;
	t->array_size = size;
	return QualifiedType{ t, false, false };
}

inline QualifiedType QualifiedType::MakeFloat() {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Float;
	return QualifiedType{ t, false, false };
}

inline QualifiedType QualifiedType::MakeBool() {
	auto t = std::make_shared<Type>();
	t->kind = Type::Kind::Bool;
	return QualifiedType{ t, false, false };
}

inline QualifiedType QualifiedType::GetCommonType(const QualifiedType& lhs, const QualifiedType& rhs) {
	if (!lhs.base_type || !rhs.base_type) return QualifiedType{};

	// Floating-point promotion (Float always wins)
	if (lhs.base_type->kind == Type::Kind::Float || rhs.base_type->kind == Type::Kind::Float) {
		if ((lhs.base_type->kind == Type::Kind::Int || lhs.base_type->kind == Type::Kind::Float) &&
			(rhs.base_type->kind == Type::Kind::Int || rhs.base_type->kind == Type::Kind::Float)) {
			return MakeFloat();
		}
	}

	// Pointer Arithmetic Rules
	// Pointer + Int -> Pointer
	if (lhs.base_type->kind == Type::Kind::Pointer && rhs.base_type->kind == Type::Kind::Int) return lhs;
	if (lhs.base_type->kind == Type::Kind::Int && rhs.base_type->kind == Type::Kind::Pointer) return rhs;

	// Pointer - Pointer -> Int 
	if (lhs.base_type->kind == Type::Kind::Pointer && rhs.base_type->kind == Type::Kind::Pointer) {
		return MakeInt();
	}
	// Array Decay to Pointer Arithmetic
	if (lhs.base_type->kind == Type::Kind::Array && rhs.base_type->kind == Type::Kind::Int) {
		return MakePointer(lhs.base_type->element, lhs.mod);
	}
	if (lhs.base_type->kind == Type::Kind::Int && rhs.base_type->kind == Type::Kind::Array) {
		return MakePointer(rhs.base_type->element, rhs.mod);
	}
	if (*lhs.base_type == *rhs.base_type) {
		QualifiedType result = lhs;
		result.mod.is_const = false;
		result.mod.is_volatile = false;
		return result;
	}

	// Invalid binary base_type combination
	return QualifiedType{};
}

struct DeclaratorModifier
{
	enum class Kind
	{
		Pointer,
		Array,
		Function
	};

	Kind kind;

	size_t array_size = 0;

	// Qualifiers specifically for this modifier level (e.g., pointer constness)
	bool is_const = false;
	bool is_volatile = false;

	std::unique_ptr<DeclaratorModifier> child = nullptr;
};

    //--------------------------------------------------------------------------------------------------------------//
    //                                                                                                              //
    //                                                      Declarator                                              //
    //                                                                                                              //
    //--------------------------------------------------------------------------------------------------------------//

struct Declarator
{
    std::string name;
    std::unique_ptr<DeclaratorModifier> modifier = nullptr;
};

struct ParsedType
{
	QualifiedType type;
    Declarator declarator;
};


struct SourceLocation
{

};
struct Identifier {
    std::string name;
    SourceLocation location; // Tracks line/col for compiler error messages
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
    int value = 0;
};
struct NodeExprBoolLit
{
    bool value = false;

};

struct NodeExprIdent
{
    std::string name;
    size_t id = 0;
    QualifiedType type;
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
    NodeExprArrayAccess // 10
> NodeExpression;
struct NodeExpr
{

    NodeExpression var;
    QualifiedType type;
};


struct NodeStatementExit
{
    NodeExpr* expr = nullptr;
};



struct NodeStatementVar
{
    size_t id = 0;
    QualifiedType type;
    std::string name;
    std::shared_ptr<Symbol> symbol = nullptr;
    NodeExpr* initializer = nullptr;

    std::optional<ConstValue> compiled_constant = std::nullopt;
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
        NodeStatementAssignment // 9
    > stat;
};


struct Parameter
{
    std::string name;                   
    QualifiedType type;
    size_t id = 0;                      
};
struct NodeFunction
{
    std::string name;
    std::vector<Parameter> parameters;
    std::optional<NodeBlock> body;
    QualifiedType retType;
    size_t stack_size = 0;
};

struct NodeTopLevel {
	std::variant<NodeFunction*, NodeStatementVar*> decl;
};

struct NodeProg {
	std::vector<NodeTopLevel> items;
};


namespace TypeInfo {


    // strictQualifiers = true: requires exact match on const/volatile flags.
    // strictQualifiers = false: checks if underlying base_types match .
	inline bool SameType(const QualifiedType& a, const QualifiedType& b, bool strictQualifiers = true) {
		if (!a.base_type || !b.base_type) return false;

		if (a.base_type == b.base_type) {
			if (strictQualifiers) {
				return a.mod.is_const == b.mod.is_const && a.mod.is_volatile == b.mod.is_volatile;
			}
			return true;
		}

		// Compare base base_type contents and qualifiers
		bool baseSame = (*a.base_type == *b.base_type);
		if (!baseSame) return false;

		if (strictQualifiers) {
			return a.mod.is_const == b.mod.is_const && a.mod.is_volatile == b.mod.is_volatile;
		}
		return true;
	}

	inline bool IsArray(const QualifiedType& qt) {
		return qt.base_type && qt.base_type->kind == Type::Kind::Array;
	}

	inline bool IsArithmetic(const QualifiedType& qt) {
		if (!qt.base_type) return false;
		return qt.base_type->kind == Type::Kind::Int ||
			qt.base_type->kind == Type::Kind::Float ||
			qt.base_type->kind == Type::Kind::Bool;
	}

	inline bool IsPointer(const QualifiedType& qt) {
		return qt.base_type && qt.base_type->kind == Type::Kind::Pointer;
	}

	inline bool IsVoidPointer(const QualifiedType& qt) {
		// Assuming a void pointer has kind Pointer whose element base_type is Void
		return qt.base_type && (qt.base_type->kind == Type::Kind::Pointer) &&
			(qt.base_type->element.base_type) && (qt.base_type->element.base_type->kind == Type::Kind::Void);
	}

	inline bool IsVoid(const QualifiedType& qt) {
		if (!qt.base_type) return false;

		std::shared_ptr<Type> current = qt.base_type;

		while (current) {
			if (current->kind == Type::Kind::Void) {
				return true;
			}

			if (current->kind != Type::Kind::Pointer) {
				return false;
			}

			current = current->element.base_type;
		}

		return false;
	}

	inline bool IsInteger(const QualifiedType& qt) {
		return qt.base_type && qt.base_type->kind == Type::Kind::Int;
	}

	inline bool IsScalar(const QualifiedType& qt) {
		return qt.base_type && (qt.base_type->kind == Type::Kind::Int ||
			qt.base_type->kind == Type::Kind::Bool ||
			qt.base_type->kind == Type::Kind::Pointer);
	}
	inline bool CanCompare(const QualifiedType& lhs, const QualifiedType& rhs)
	{
		if (!lhs.base_type || !rhs.base_type) return false;

		if (*lhs.base_type == *rhs.base_type && lhs.mod.is_const == rhs.mod.is_const && lhs.mod.is_volatile == rhs.mod.is_volatile) return true;

		if (IsArithmetic(lhs) && IsArithmetic(rhs)) return true;

		if (IsPointer(lhs) && IsPointer(rhs))
		{
			// Any pointer can be compared with void* in C
			if (IsVoidPointer(lhs) || IsVoidPointer(rhs)) return true;

			// Pointers to compatible element base_types
			if (lhs.base_type->element.base_type && rhs.base_type->element.base_type) {
				return *lhs.base_type->element.base_type == *rhs.base_type->element.base_type;
			}
		}

		if ((IsPointer(lhs) && IsInteger(rhs)) || (IsInteger(lhs) && IsPointer(rhs))) {
			return true;
		}

		return false;
	}
	inline size_t CalculateSizeInBytes(const QualifiedType& qt)
	{
		if (!qt.base_type) return 0;
		switch (qt.base_type->kind)
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
			return qt.base_type->array_size * CalculateSizeInBytes(qt.base_type->element);
		case Type::Kind::Void:
			return 0;
		default:
			std::cerr << "Unknown base_type encountered in size calculation." << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	inline std::string TypeToString(const QualifiedType& qt)
	{
		if (!qt.base_type) { return "Invalid base_type"; }

		std::string prefix = "";
		if (qt.mod.is_const) prefix += "const ";
		if (qt.mod.is_volatile) prefix += "volatile ";

		switch (qt.base_type->kind)
		{
		case Type::Kind::Int:
			return prefix + "int";
		case Type::Kind::Bool:
			return prefix + "bool";
		case Type::Kind::Float:
			return prefix + "float";
		case Type::Kind::Void:
			return prefix + "void";
		case Type::Kind::Pointer:
			if (qt.base_type->element.base_type)
				return prefix + TypeToString(qt.base_type->element) + "*";
			return prefix + "void*";
		case Type::Kind::Array:
			if (qt.base_type->element.base_type)
				return prefix + TypeToString(qt.base_type->element) +
				"[" + std::to_string(qt.base_type->array_size) + "]";
		}

		return prefix + "unknown";
	}

	inline size_t GetTypeSize(const QualifiedType& qt) {
		if (!qt.base_type) return 0;
		if (qt.base_type->kind == Type::Kind::Pointer) {
			return 8; // 64-bit architecture pointer size
		}
		return CalculateSizeInBytes(qt);
	}

	inline size_t GetTypeAlignment(const QualifiedType& qt)
	{
		if (!qt.base_type) return 0;
		switch (qt.base_type->kind)
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
			std::cerr << "Unknown base_type encountered in alignment calculation." << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	inline std::vector<size_t> GetArrayDimensions(const QualifiedType& qt)
	{
		std::vector<size_t> dimensions;
		QualifiedType current = qt;

		while (current.base_type)
		{
			if (current.base_type->kind == Type::Kind::Array) {
				dimensions.push_back(current.base_type->array_size);
			}
			current = current.base_type->element;
		}

		return dimensions;
	}

	inline QualifiedType SetArrayDimensionSize(const QualifiedType& qt, size_t dimTarget, size_t newDim)
	{
		if (!qt.base_type) return QualifiedType{};

		QualifiedType root = qt;
		root.base_type = std::make_shared<Type>(*qt.base_type);
		auto currentType = root.base_type;

		size_t pos = 0;
		while (pos < dimTarget && currentType && currentType->element.base_type)
		{
			currentType->element.base_type = std::make_shared<Type>(*currentType->element.base_type);
			currentType = currentType->element.base_type;
			pos++;
		}

		if (currentType && pos == dimTarget)
		{
			currentType->array_size = newDim;
		}

		return root;
	}

	inline QualifiedType GetBaseType(const QualifiedType& qt)
	{
		QualifiedType current = qt;
		QualifiedType t = qt;
		while (current.base_type && (current.base_type->kind == Type::Kind::Array))
		{
			current = current.base_type->element;
			t = current;
		}
		return t;
	}

	inline size_t GetTotalArraySize(const QualifiedType& qt)
	{
		if (!qt.base_type || !qt.base_type->element.base_type) return 0;
		size_t size = 1;

		auto dimensions = GetArrayDimensions(qt);
		for (auto dim : dimensions)
		{
			size *= dim;
		}
		return size * GetTypeSize(qt.base_type->element);
	}

	inline QualifiedType GetPointeeType(const QualifiedType& qt)
	{
		assert(qt.base_type && qt.base_type->kind == Type::Kind::Pointer);
		return qt.base_type->element;
	}

	inline size_t GetPointerDepth(const QualifiedType& qt)
	{
		size_t depth = 0;
		QualifiedType current = qt;

		while (current.base_type && current.base_type->kind == Type::Kind::Pointer)
		{
			++depth;
			current = current.base_type->element;
		}

		return depth;
	}

	inline size_t GetPointeeSize(const QualifiedType& qt)
	{
		return GetTypeSize(qt);
	}

	inline QualifiedType MakePointerType(const QualifiedType& qt)
	{
		Type result;
		result.kind = Type::Kind::Pointer;
		result.element = qt;
		return QualifiedType{ std::make_shared<Type>(result), false, false };
	}
}


class ValueID
{
public:
    static size_t Create()
    {
        return counter++;
    }
    static constexpr int INVALID_ID = -1;
    
private:
    inline static size_t counter = 0;
};