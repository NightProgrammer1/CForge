#pragma once
#include <array>
namespace Register {													
	enum class Reg : size_t { RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11, RBX, R12, R13, R14, R15, RBP, RSP };
	constexpr size_t NUM_OF_REGISTERS = 8;
	constexpr size_t ABI_ARGS = 6;

	static constexpr std::array<Reg, NUM_OF_REGISTERS> registers = {
			Reg::RAX,
			Reg::RCX,
			Reg::RDX,
			Reg::RSI,
			Reg::RDI,
			Reg::R8,
			Reg::R9,
			Reg::R10,
	};
	static const std::array<Reg, ABI_ARGS> PARAM_REGS = { Reg::RDI, Reg::RSI, Reg::RDX, Reg::RCX, Reg::R8, Reg::R9 };
	static constexpr std::array<Register::Reg, 5> calleSavedRegisters = { Reg::RBX, Reg::R12, Reg::R13, Reg::R14, Reg::R15 };

	

	struct StackObject
	{
		size_t size = 0;
		size_t alignment = 8;
		size_t id = 0;
		size_t offset = 0;
		bool operator==(const StackObject& other) const
		{
			return id == other.id;
		}
	};
	struct VReg
	{
		size_t id = 0;
		size_t size = 0;
		bool operator==(const VReg& other) const
		{
			return id == other.id;
		}
	};


	struct Storage
	{
		std::variant<StackObject, VReg> data;
		bool operator==(const Storage& other) const
		{
			return std::visit([](const auto& object, const auto& object2) {
				return object.id == object2.id;
				}, data, other.data);
		}
		bool operator<(const Storage& other) const
		{
			return std::visit([](const auto& object, const auto& object2) {
				return object.id < object2.id;
				}, data, other.data);
		}
	};
	constexpr std::string_view RegToString(const Reg reg)
	{
		switch (reg)
		{
		case Reg::RAX: return "rax";
		case Reg::RCX: return "rcx";
		case Reg::RDX: return "rdx";
		case Reg::RSI: return "rsi";
		case Reg::RDI: return "rdi";
		case Reg::R8:  return "r8";
		case Reg::R9:  return "r9";
		case Reg::R10: return "r10";
		case Reg::R11: return "r11";
		case Reg::RBX: return "rbx";
		case Reg::R12: return "r12";
		case Reg::R13: return "r13";
		case Reg::R14: return "r14";
		case Reg::R15: return "r15";

		default:       return "unknown"; 
		}
	}

}


namespace Memory
{
	enum class OperandSize {
		Byte = 1,
		Word = 2,
		Dword = 4, // int (32-bit)
		Qword = 8  // pointers, 64-bit ints
	};

	constexpr char* GetSizeSpecifier(const OperandSize& size) {
		switch (size) {
		case OperandSize::Byte:  return "byte";
		case OperandSize::Word:  return "word";
		case OperandSize::Dword: return "dword";
		case OperandSize::Qword: return "qword";
		}
		return "dword";
	}
	constexpr std::string_view GetPhysicalRegName(const Register::Reg& reg, const Memory::OperandSize& size) {

		using namespace Memory;
		switch (reg) {
		case Register::Reg::RAX:
			switch (size) {
			case OperandSize::Byte:  return "al";
			case OperandSize::Word:  return "ax";
			case OperandSize::Dword: return "eax";
			case OperandSize::Qword: return "rax";
			}
			break;
		case Register::Reg::RBX:
			switch (size) {
			case OperandSize::Byte:  return "bl";
			case OperandSize::Word:  return "bx";
			case OperandSize::Dword: return "ebx";
			case OperandSize::Qword: return "rbx";
			}
			break;
		case Register::Reg::RCX:
			switch (size) {
			case OperandSize::Byte:  return "cl";
			case OperandSize::Word:  return "cx";
			case OperandSize::Dword: return "ecx";
			case OperandSize::Qword: return "rcx";
			}
			break;
		case Register::Reg::RDX:
			switch (size) {
			case OperandSize::Byte:  return "dl";
			case OperandSize::Word:  return "dx";
			case OperandSize::Dword: return "edx";
			case OperandSize::Qword: return "rdx";
			}
			break;
		case Register::Reg::RSI:
			switch (size) {
			case OperandSize::Byte:  return "sil";
			case OperandSize::Word:  return "si";
			case OperandSize::Dword: return "esi";
			case OperandSize::Qword: return "rsi";
			}
			break;
		case Register::Reg::RDI:
			switch (size) {
			case OperandSize::Byte:  return "dil";
			case OperandSize::Word:  return "di";
			case OperandSize::Dword: return "edi";
			case OperandSize::Qword: return "rdi";
			}
			break;
		case Register::Reg::RBP:
			switch (size) {
			case OperandSize::Byte:  return "bpl";
			case OperandSize::Word:  return "bp";
			case OperandSize::Dword: return "ebp";
			case OperandSize::Qword: return "rbp";
			}
			break;
		case Register::Reg::RSP:
			switch (size) {
			case OperandSize::Byte:  return "spl";
			case OperandSize::Word:  return "sp";
			case OperandSize::Dword: return "esp";
			case OperandSize::Qword: return "rsp";
			}
			break;
		case Register::Reg::R8:
			switch (size) {
			case OperandSize::Byte:  return "r8b";
			case OperandSize::Word:  return "r8w";
			case OperandSize::Dword: return "r8d";
			case OperandSize::Qword: return "r8";
			}
			break;
		case Register::Reg::R9:
			switch (size) {
			case OperandSize::Byte:  return "r9b";
			case OperandSize::Word:  return "r9w";
			case OperandSize::Dword: return "r9d";
			case OperandSize::Qword: return "r9";
			}
			break;
		case Register::Reg::R10:
			switch (size) {
			case OperandSize::Byte:  return "r10b";
			case OperandSize::Word:  return "r10w";
			case OperandSize::Dword: return "r10d";
			case OperandSize::Qword: return "r10";
			}
			break;
		case Register::Reg::R11:
			switch (size) {
			case OperandSize::Byte:  return "r11b";
			case OperandSize::Word:  return "r11w";
			case OperandSize::Dword: return "r11d";
			case OperandSize::Qword: return "r11";
			}
			break;
		case Register::Reg::R12:
			switch (size) {
			case OperandSize::Byte:  return "r12b";
			case OperandSize::Word:  return "r12w";
			case OperandSize::Dword: return "r12d";
			case OperandSize::Qword: return "r12";
			}
			break;
		case Register::Reg::R13:
			switch (size) {
			case OperandSize::Byte:  return "r13b";
			case OperandSize::Word:  return "r13w";
			case OperandSize::Dword: return "r13d";
			case OperandSize::Qword: return "r13";
			}
			break;
		case Register::Reg::R14:
			switch (size) {
			case OperandSize::Byte:  return "r14b";
			case OperandSize::Word:  return "r14w";
			case OperandSize::Dword: return "r14d";
			case OperandSize::Qword: return "r14";
			}
			break;
		case Register::Reg::R15:
			switch (size) {
			case OperandSize::Byte:  return "r15b";
			case OperandSize::Word:  return "r15w";
			case OperandSize::Dword: return "r15d";
			case OperandSize::Qword: return "r15";
			}
			break;
		}
		return "rax";
	}


	constexpr OperandSize GetDestinationSize(const OperandSize& size)
	{
		Memory::OperandSize accessSize = (size >= Memory::OperandSize::Qword)
			? Memory::OperandSize::Qword
			: Memory::OperandSize::Dword;

		return accessSize;
	}

}
namespace std
{
	template<>
	struct hash<Register::VReg> {
		size_t operator()(const Register::VReg& reg) const noexcept {
			return std::hash<size_t>{}(reg.id);
		}
	};

	template<>
	struct hash<Register::StackObject> {
		size_t operator()(const Register::StackObject& object) const noexcept {
			size_t value = static_cast<size_t>(object.id); // Or standard hash depending on type
			return std::hash<size_t>{}(value);
		}
	};

	template<>
	struct hash<Register::Storage> {
		size_t operator()(const Register::Storage& storage) const noexcept {
			return std::visit([](const auto& object) -> size_t {
				return std::hash<std::decay_t<decltype(object)>>{}(object);
				}, storage.data);
		}
	};
}