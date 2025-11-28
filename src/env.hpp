#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

// basic types
enum class LatteTypeKind {
    Int,
    Bool,
    String,
    Void,
    Unknown
};

struct LatteType {
    LatteTypeKind kind;

    bool operator==(const LatteType& other) const noexcept
    {
        return kind == other.kind;
    }

    bool operator!=(const LatteType& other) const noexcept
    {
        return !(*this == other);
    }

    static LatteType Int() { return { LatteTypeKind::Int }; }
    static LatteType Bool() { return { LatteTypeKind::Bool }; }
    static LatteType String() { return { LatteTypeKind::String }; }
    static LatteType Void() { return { LatteTypeKind::Void }; }
    static LatteType Unknown() { return { LatteTypeKind::Unknown }; }
};

struct VarInfo {
    LatteType type;
};

struct FunInfo {
    LatteType result;
    std::vector<LatteType> args;
};

// env = symbol array for functions and variables
class Env {
public:
    Env();

    // global functions
    void enterFunction(const std::string& name, const FunInfo& info);
    std::optional<FunInfo> lookupFunction(const std::string& name) const;

    // variables with scope range
    void pushScope();
    void popScope();

    void declareVar(const std::string& name, const VarInfo& info);

    // search through every scope
    std::optional<VarInfo> lookupVar(const std::string& name) const;

    // search only current scope
    bool isVarDeclaredInCurrentScope(const std::string& name) const;

private:
    std::unordered_map<std::string, FunInfo> globalFunctions_;
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
};
