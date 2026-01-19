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
    Array,  // T[] 1d array type
    Null    // null
};

struct LatteType {
    LatteTypeKind kind = LatteTypeKind::Unknown;
    std::string name; // for Class
    std::shared_ptr<LatteType> elem; // for Array

    LatteType() = default;
    explicit LatteType(LatteTypeKind k) : kind(k) {}

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

    // constructors for common types
    static LatteType Int()     { return LatteType(LatteTypeKind::Int); }
    static LatteType Bool()    { return LatteType(LatteTypeKind::Bool); }
    static LatteType String()  { return LatteType(LatteTypeKind::String); }
    static LatteType Void()    { return LatteType(LatteTypeKind::Void); }
    static LatteType Unknown() { return LatteType(LatteTypeKind::Unknown); }

    static LatteType Class(std::string n)
    {
        LatteType t(LatteTypeKind::Class);
        t.name = std::move(n);
        return t;
    }

    static LatteType Array(LatteType element)
    {
        LatteType t(LatteTypeKind::Array);
        t.elem = std::make_shared<LatteType>(std::move(element)); // heap store for recursion
        return t;
    }

    static LatteType Null()
    {
        return LatteType(LatteTypeKind::Null); // null literal type
    }

    bool isRef() const noexcept
    {
        // string is treated like a reference for null/equality rules
        return kind == LatteTypeKind::String
            || kind == LatteTypeKind::Class
            || kind == LatteTypeKind::Array;
    }
};

struct VarInfo {
    LatteType type; // variable static type
};

struct FunInfo {
    LatteType result; // return type
    std::vector<LatteType> args;
};

// classes
struct FieldInfo {
    LatteType type; // field type
    int index = -1; // backend offset / index
};

struct MethodInfo {
    FunInfo sig; // return type + arg types
    int index = -1;
};

struct ClassInfo {
    std::string name; // class name
    std::optional<std::string>  base; // optional base class name
    std::unordered_map<std::string, FieldInfo> fields; // fields by name
    std::unordered_map<std::string, MethodInfo> methods; // methods by name
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

    // classes
    bool tryEnterClass(const ClassInfo& c);
    std::optional<ClassInfo> lookupClass(const std::string& name) const;

    // lookup with inheritance
    std::optional<FieldInfo>  lookupField (const std::string& className, const std::string& fieldName) const;
    std::optional<MethodInfo> lookupMethod(const std::string& className, const std::string& methodName) const;

    // mutable access (for collectSignatures)
    ClassInfo& getClassRef(const std::string& name);

    // layout helpers (extends): total (self + bases) counts
    int countAllFields (const std::string& className) const;
    int countAllMethods(const std::string& className) const;

    // objects1 helpers: check only bases (exclude self)
    bool hasFieldInBases (const std::string& className, const std::string& fieldName) const;
    bool hasMethodInBases(const std::string& className, const std::string& methodName) const;

private:
    std::unordered_map<std::string, FunInfo> globalFunctions_;
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, ClassInfo> classes_;

    // internal helper
    const ClassInfo* getClassPtr(const std::string& name) const;
};
