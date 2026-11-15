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
#include <cstdint>

struct NodeExpr;
struct Symbol;

struct Declarator;

//--------------------------------------------------------------------------------------------------------------//
//                                                                                                              //
//                                                Type information                                              //
//                                                                                                              //
//--------------------------------------------------------------------------------------------------------------//

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

	static QualifiedType MakePointer(const QualifiedType& target, const Modifier& mod);
	static QualifiedType MakeArray(const QualifiedType& elem, size_t size);
};

struct Type {
	enum class Kind { Void, Int, Float, Char, Short, Bool, Long, LongLong, Double, Pointer, Array, Function };
	enum class IntegerRank {
		Bool = 0,
		Char,       // covers char, signed char, unsigned char
		Short,      // covers short, unsigned short
		Int,        // covers int, unsigned int
		Long,       // covers long, unsigned long
		LongLong    // covers long long, unsigned long long
	};
	Kind kind = Kind::Void;

	QualifiedType element;

	size_t array_size = 0;

	bool is_signed = true;

	std::vector<QualifiedType> param_types;
	IntegerRank GetIntegerRank() {
		switch (kind) {
		case Type::Kind::Bool:
			return IntegerRank::Bool;
		case Type::Kind::Char:
			return IntegerRank::Char;
		case Type::Kind::Short:
			return IntegerRank::Short;
		case Type::Kind::Int:
			return IntegerRank::Int;
		default:
			return IntegerRank::Int; // Fallback or non-integer type
		}
	}
	std::string ToString() const;

	static std::shared_ptr<Type> GetCommonType(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs);


	static std::shared_ptr<Type> MakeFloat() {
		return std::make_shared<Type>(Kind::Float);
	}
	static std::shared_ptr<Type> MakeInt(bool is_unsigned = false) {
		return std::make_shared<Type>(Kind::Int, QualifiedType{}, !is_unsigned);

	}
	static std::shared_ptr<Type> MakeChar() {
		return std::make_shared<Type>(Kind::Char);

	}
	static std::shared_ptr<Type> MakeVoid() {
		return std::make_shared<Type>(Kind::Void);

	}
	static std::shared_ptr<Type> MakeBool() {
		return std::make_shared<Type>(Kind::Bool);

	}
	static std::shared_ptr<Type> MakeShort(bool is_unsigned = false) {
		return std::make_shared<Type>(Kind::Bool, QualifiedType{}, !is_unsigned);

	}
	static std::shared_ptr<Type> MakeLong(bool is_unsigned = false) {
		return std::make_shared<Type>(Kind::Long, QualifiedType{}, !is_unsigned);

	}
	static std::shared_ptr<Type> MakeLongLong(bool is_unsigned = false) {
		return std::make_shared<Type>(Kind::LongLong, QualifiedType{}, !is_unsigned);

	}

	static std::shared_ptr<Type> MakePointer(const std::shared_ptr<Type>& target) {
		return std::make_shared<Type>(Kind::Pointer, QualifiedType{ target });
	}
	size_t Size() const
	{
		switch (kind)
		{
		case Type::Kind::Char:
			return 1;
		case Type::Kind::Short:
			return 2;
		case Type::Kind::Int:
			return 4;
		case Type::Kind::Bool:
			return 1;
		case Type::Kind::Float:
			return 4;
		case Type::Kind::Double:
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
	std::string sign_prefix = "";
	if (!is_signed && (kind == Type::Kind::Int || kind == Type::Kind::Short || kind == Type::Kind::Char)) {
		sign_prefix = "unsigned ";
	}

	switch (kind)
	{
	case Type::Kind::Char:
		return sign_prefix + "char";
	case Type::Kind::Int:
		return sign_prefix + "int";
	case Type::Kind::Short:
		return sign_prefix + "short";
	case Type::Kind::Bool:
		return "bool";
	case Type::Kind::Float:
		return "float";
	case Type::Kind::Double:
		return "double";
	case Type::Kind::Void:
		return "void";
	case Type::Kind::Pointer:
		if (element.base_type)
			return element.ToString() + "*";
		return "void*";
	case Type::Kind::Array:
		if (element.base_type)
			return element.ToString() + "[" + std::to_string(array_size) + "]";
		return "unknown[]";
	}
	return "unknown";
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

inline std::shared_ptr<Type> Type::GetCommonType(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs) {
	if (!lhs || !rhs) return nullptr;

	// 1. Exact match check
	if (*lhs == *rhs) {
		return lhs;
	}

	// 2. Floating-point promotion (Float always wins over integers)
	if (lhs->kind == Kind::Float || rhs->kind == Kind::Float) {
		return MakeFloat();
	}

	// 3. Pointer Arithmetic Rules
	// Pointer + Int -> Pointer
	if (lhs->kind == Kind::Pointer && (rhs->kind == Kind::Int || rhs->kind == Kind::Short || rhs->kind == Kind::Char)) return lhs;
	if ((lhs->kind == Kind::Int || lhs->kind == Kind::Short || lhs->kind == Kind::Char) && rhs->kind == Kind::Pointer) return rhs;

	// Pointer - Pointer -> Int 
	if (lhs->kind == Kind::Pointer && rhs->kind == Kind::Pointer) {
		return MakeInt(); // or MakeSignedInt() depending on your type factory
	}

	// Array Decay to Pointer Arithmetic
	if (lhs->kind == Kind::Array && (rhs->kind == Kind::Int || rhs->kind == Kind::Short || rhs->kind == Kind::Char)) {
		return MakePointer(lhs->element.base_type);
	}
	if ((lhs->kind == Kind::Int || lhs->kind == Kind::Short || lhs->kind == Kind::Char) && rhs->kind == Kind::Array) {
		return MakePointer(rhs->element.base_type);
	}

	// 4. Integer Promotions (Types smaller than int promote to int)

	bool lhs_unsigned = !lhs->is_signed;
	bool rhs_unsigned = !rhs->is_signed;
	IntegerRank lhs_rank = lhs->GetIntegerRank();
	IntegerRank rhs_rank = rhs->GetIntegerRank();

	// Enforce integer promotion: char/short/bool promote at least to int
	if (lhs_rank < IntegerRank::Int) {
		lhs_rank = IntegerRank::Int;
	}
	if (rhs_rank < IntegerRank::Int) {
		rhs_rank = IntegerRank::Int;
	}

	// Usual Arithmetic Conversion Rules (Unsigned vs Signed rank comparison)
	bool result_is_unsigned =
		(lhs_unsigned && lhs_rank >= rhs_rank) ||
		(rhs_unsigned && rhs_rank >= lhs_rank);

	// Determine the common rank (the higher of the two)
	IntegerRank common_rank = (lhs_rank > rhs_rank) ? lhs_rank : rhs_rank;

	// Construct and return the resulting promoted integer type based on rank and sign
	switch (common_rank) {
	case IntegerRank::Int:
	case IntegerRank::Char:   // Promoted up to Int
	case IntegerRank::Short:  // Promoted up to Int
	case IntegerRank::Bool:   // Promoted up to Int
		return MakeInt(result_is_unsigned);
	case IntegerRank::Long:
		return MakeLong(result_is_unsigned);
	case IntegerRank::LongLong:
		return MakeLongLong(result_is_unsigned);
	default:
		return MakeInt(false);
	}

	return nullptr;
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

	// Qualifiers specifically for this modifier level 
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

//--------------------------------------------------------------------------------------------------------------//
//                                                                                                              //
//                                                      Constant Value                                          //
//                                                                                                              //
//--------------------------------------------------------------------------------------------------------------//
struct ConstValue {

    inline static size_t copy_count = 0;
	struct Pointer {
		size_t variable_id;
		std::vector<size_t> indices; // Tracks paths
		int64_t element_offset = 0;
	};
	using Scalar = std::variant<
		bool,          // 1-bit boolean (true / false)
		int8_t,        // Signed 8-bit integer   (signed char)
		uint8_t,       // Unsigned 8-bit integer (unsigned char)
		int16_t,       // Signed 16-bit integer  (short)
		uint16_t,      // Unsigned 16-bit integer(unsigned short)
		int32_t,       // Signed 32-bit integer  (int)
		uint32_t,      // Unsigned 32-bit integer(unsigned int)
		int64_t,       // Signed 64-bit integer  (long / long long on x86-64 Linux)
		uint64_t,      // Unsigned 64-bit integer(unsigned long / unsigned long long)
		float,         // 32-bit single-precision float
		double         // 64-bit double-precision float
	>;


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

	static ConstValue::Scalar cast_to_type(const ConstValue::Scalar& val, const std::shared_ptr<Type>& target) {
		return std::visit([&target](auto&& scalar) -> ConstValue::Scalar {
			// Floating-point targets
			if (target->kind == Type::Kind::Float) {
				return static_cast<float>(scalar);
			}
			if (target->kind == Type::Kind::Double) {
				return static_cast<double>(scalar);
			}

			size_t size = target->Size(); // Returns 1, 2, 4, or 8 bytes

			if (!target->is_signed) {
				switch (size) {
				case 1:  return static_cast<uint8_t>(scalar);
				case 2:  return static_cast<uint16_t>(scalar);
				case 4:  return static_cast<uint32_t>(scalar);
				case 8:
				default: return static_cast<uint64_t>(scalar);
				}
			}
			else {
				switch (size) {
				case 1:  return static_cast<int8_t>(scalar);
				case 2:  return static_cast<int16_t>(scalar);
				case 4:  return static_cast<int32_t>(scalar);
				case 8:
				default: return static_cast<int64_t>(scalar);
				}
			}
			}, val);
	}

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
		case Type::Kind::Char:
			return 1;
		case Type::Kind::Short:
			return 2;
		case Type::Kind::Int:
			return 4;
		case Type::Kind::Bool:
			return 1;
		case Type::Kind::Float:
			return 4;
		case Type::Kind::Double:
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
		case Type::Kind::Char:
			return prefix + "char";
		case Type::Kind::Short:
			return prefix + "short";
		case Type::Kind::Int:
			return prefix + "int";
		case Type::Kind::Bool:
			return prefix + "bool";
		case Type::Kind::Float:
			return prefix + "float";
		case Type::Kind::Double:
			return prefix + "double";
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
		case Type::Kind::Char:
			return 1;
		case Type::Kind::Short:
			return 2;
		case Type::Kind::Int:
			return alignof(int);
		case Type::Kind::Bool:
			return 1;
		case Type::Kind::Float:
			return 4;
		case Type::Kind::Double:
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
		return QualifiedType{ std::make_shared<Type>(Type::Kind::Pointer, qt), false, false };
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