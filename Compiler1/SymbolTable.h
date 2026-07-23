#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "parser.h"


struct Symbol {
    std::string name;
    Type type;
    size_t stackOffset; 
};

class SymbolTable
{
public:
    size_t stackOffset = 0; //variable offsets relative to rbp

    void EnterScope() {
        scopes.emplace_back();
    }

    void ExitScope() {
        scopes.pop_back();
    }

    bool DeclareVariable(const std::string& name, Type type) {
        if (scopes.empty()) return false;

        // Prevent redeclaration in the CURRENT scope
        if (scopes.back().contains(name)) {
            return false;
        }
        stackOffset += 8;
        scopes.back()[name] = Symbol{ name, type, stackOffset };
        return true;
    }

    std::optional<Symbol> LookupVariable(const std::string& name) const {
        // Search from innermost scope outwards
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;

};