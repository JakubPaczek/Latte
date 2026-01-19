#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>

// types
enum class LatteTypeKind {
    Int,
    Bool,
    String,
    Void,
    Unknown,

    // extensions
    Class,  // Ident
    Array,  // T[]
    Null    // null
};

struct LatteType {
    LatteTypeKind kind = LatteTypeKind::Unknown;

    std::string name; // for Class

    std::shared_ptr<LatteType> elem; // for Array

    bool operator==(const LatteType& other) const noexcept
    {
        if (kind != other.kind) return false;
        if (kind == LatteTypeKind::Class) return name == other.name;
        if (kind == LatteTypeKind::Array)
        {
            if (!elem || !other.elem) return false;
            return *elem == *other.elem;
        }
        return true;
    }

    bool operator!=(const LatteType& other) const noexcept
    {
        return !(*this == other);
    }

    static LatteType Int() { return { LatteTypeKind::Int }; }
    static LatteType Bool()     { return { LatteTypeKind::Bool }; }
    static LatteType String()   { return { LatteTypeKind::String }; }
    static LatteType Void()     { return { LatteTypeKind::Void }; }
    static LatteType Unknown() { return { LatteTypeKind::Unknown }; }

    static LatteType Class(std::string n)
    {
        LatteType t;
        t.kind = LatteTypeKind::Class;
        t.name = std::move(n);
        return t;
    }

    static LatteType Array(LatteType element)
    {
        LatteType t;
        t.kind = LatteTypeKind::Array;
        t.elem = std::make_shared<LatteType>(std::move(element));
        return t;
    }

    static LatteType Null()
    {
        LatteType t;
        t.kind = LatteTypeKind::Null;
        return t;
    }

    bool isRef() const noexcept
    {
        return kind == LatteTypeKind::String
            || kind == LatteTypeKind::Class
            || kind == LatteTypeKind::Array;
    }
};

struct VarInfo {
    LatteType type;
};

struct FunInfo {
    LatteType result;
    std::vector<LatteType> args;
};

// classess
struct FieldInfo {
    LatteType type;
    int index = -1; //backend offset
};

struct MethodInfo {
    FunInfo sig;
    int index = 1;
};

struct ClassInfo {
    std::string name;
    std::optional<std::string>  base; // "extends"
    std::unordered_map<std::string, FieldInfo> fields;
    std::unordered_map<std::string, MethodInfo> methods;
};

// env
class Env {
public:
    Env();

    // global functions
    void enterFunction(const std::string& name, const FunInfo& info);
    std::optional<FunInfo> lookupFunction(const std::string& name) const;

    bool tryEnterFunction(const std::string& name, const FunInfo& info);

    // variables with scope
    void pushScope();
    void popScope();

    void declareVar(const std::string& name, const VarInfo& info);

    std::optional<VarInfo> lookupVar(const std::string& name) const;
    bool isVarDeclaredInCurrentScope(const std::string& name) const;

    bool tryDeclareVar(const std::string& name, const VarInfo& info);

    // classess
    bool tryEnterClass(const ClassInfo& c);
    std::optional<ClassInfo> lookupClass(const std::string& name) const;

    std::optional<FieldInfo> lookupField(const std::string& className, const std::string& fieldName) const;
    std::optional<MethodInfo> lookupMethod(const std::string& className, const std::string& methodName) const;
    ClassInfo& getClassRef(const std::string& name);

private:
    std::unordered_map<std::string, FunInfo> globalFunctions_;
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;

    std::unordered_map<std::string, ClassInfo> classes_;
};
