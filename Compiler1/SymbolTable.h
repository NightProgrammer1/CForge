#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "parser.h"


struct Symbol {
    std::string name;
    std::shared_ptr<Type> type = nullptr;
    size_t id = 0;
    size_t stackOffset = 0;
    bool isAddressTaken = false;

};

struct FunctionSymbol {
    std::shared_ptr<Type> returnType;
    std::vector<std::shared_ptr<Type>> paramTypes;
    bool hasBody = false;
    bool hasReturn = false;

    
};

class SymbolTable
{
public:

    void EnterScope() {

        scopes.emplace_back();
    }

    void ExitScope() {
        scopes.pop_back();
    }

    void SetFunctionHasReturn(const std::string& name, bool value) {
        if (functions.contains(name)) {
            functions[name].hasReturn = value;
        }
    }

   
    bool DeclareParameter(const std::string& name, const std::shared_ptr<Type>& type_info, const size_t& id)
    {
        if (scopes.empty()) return false;

        if (scopes.back().contains(name)) {
            return false;
        }
        auto symbol = Symbol{ name, type_info, id , 0 };// add more than 6 param stack logic

        scopes.back()[name] = std::make_shared<Symbol>(symbol);
        return true;
    }
    bool DeclareVariable(const std::string& name, const std::shared_ptr<Type>& type_info, const size_t& id) {
        if (scopes.empty()) return false;

        // Prevent redeclaration in the CURRENT scope
        if (scopes.back().contains(name)) {
            return false;
        }
        stackOffset += AlignUp(TypeInfo::GetTypeSize(*type_info), TypeInfo::GetTypeAlignment(*type_info));
        auto symbol = Symbol{ name, type_info, id , stackOffset };
        
        scopes.back()[name] = std::make_shared<Symbol>(symbol);
        return true;
    }
    bool DeclareVariable(std::shared_ptr<Symbol>& symbol) {

        if (!symbol) { return false; }
        if (scopes.empty()) return false;

        // Prevent redeclaration in the CURRENT scope
        if (scopes.back().contains(symbol->name)) {
            return false;
        }

        stackOffset += AlignUp(TypeInfo::GetTypeSize(*symbol->type), TypeInfo::GetTypeAlignment(*symbol->type));
        symbol->stackOffset = stackOffset;
        scopes.back()[symbol->name] = symbol;
        return true;
    }

    void ModifyVariableSymbol(const Symbol& symbol)
    {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(symbol.name);
            if (found != it->end()) {
                *found->second = symbol;
                break;
            }
        }
    }

    size_t GetCurrentStackSize()
    {
		return stackOffset;
    }
    bool DeclareFunction(const std::string& name, std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> paramTypes, bool hasBody) {
        if (functions.contains(name)) {
            auto& existing = functions.at(name);
            if (existing.paramTypes != paramTypes || existing.returnType != returnType) {
                return false; // Signature mismatch
            }
            if (existing.hasBody && hasBody) {
                return false; 
            }
            if (hasBody) {
                existing.hasBody = true; 
            }
            return true;
        }

        functions[name] = FunctionSymbol{ returnType, paramTypes, hasBody };


        return true;
    }
    std::shared_ptr<Symbol> LookupVariable(const std::string& name) const {
        // Search from innermost scope outwards
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    std::optional<FunctionSymbol> LookupFunction(const std::string& name)
    {
        if (functions.contains(name)) {
            return functions.at(name);
        }
        return std::nullopt;
    }

    size_t stackOffset = 0; //variable offsets relative to rbp
private:

    size_t AlignUp(size_t value, size_t alignment)
    {
        if (alignment == 0) return value;
        return (value + alignment - 1) & ~(alignment - 1);
    }
    std::unordered_map<std::string, FunctionSymbol> functions;
    std::vector<std::unordered_map<std::string, std::shared_ptr<Symbol>>> scopes;
    FunctionSymbol* currentFunctionSymbol = nullptr;
    size_t _currentId = 0;
};