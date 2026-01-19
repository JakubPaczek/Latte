#include "env.h"
#include "latte_error.h"

#include <stdexcept>
#include <unordered_map>

Env::Env()
{
    // global scope for variables
    pushScope(); // start with one scope
}

// functions
void Env::enterFunction(const std::string& name, const FunInfo& info)
{
    globalFunctions_[name] = info; // insert or overwrite (no duplicate check)
}

bool Env::tryEnterFunction(const std::string& name, const FunInfo& info)
{
    return globalFunctions_.emplace(name, info).second; // insert only if not present
}

std::optional<FunInfo> Env::lookupFunction(const std::string& name) const
{
    auto it = globalFunctions_.find(name);
    if (it == globalFunctions_.end())
        return std::nullopt; // not found
    return it->second;
}

// scopes
void Env::pushScope()
{
    scopes_.emplace_back(); // new empty map: name -> VarInfo
}

void Env::popScope()
{
    if (!scopes_.empty())
        scopes_.pop_back(); // drop locals from this scope
}

void Env::declareVar(const std::string& name, const VarInfo& info)
{
    if (scopes_.empty())
        pushScope(); // safety
    scopes_.back()[name] = info;
}

bool Env::tryDeclareVar(const std::string& name, const VarInfo& info)
{
    if (scopes_.empty())
        pushScope(); // safety
    auto& cur = scopes_.back();
    return cur.emplace(name, info).second;
}

std::optional<VarInfo> Env::lookupVar(const std::string& name) const
{
    // search from innermost scope to outermost scope
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
            return found->second; // first match wins (shadowing)
    }
    return std::nullopt;
}

bool Env::isVarDeclaredInCurrentScope(const std::string& name) const
{
    if (scopes_.empty())
        return false;
    const auto& current = scopes_.back();
    return current.find(name) != current.end();// only checks top scope
}

// classes
bool Env::tryEnterClass(const ClassInfo& c)
{
    return classes_.emplace(c.name, c).second; // insert only if not present
}

std::optional<ClassInfo> Env::lookupClass(const std::string& name) const
{
    auto it = classes_.find(name);
    if (it == classes_.end())
        return std::nullopt;
    return it->second;
}

ClassInfo& Env::getClassRef(const std::string& name)
{
    auto it = classes_.find(name);
    if (it == classes_.end())
        throw LatteError("Internal: class not found: " + name, 0);
    return it->second;
}

const ClassInfo* Env::getClassPtr(const std::string& name) const
{
    auto it = classes_.find(name);
    if (it == classes_.end()) return nullptr;
    return &it->second;
}

std::optional<FieldInfo> Env::lookupField(const std::string& className, const std::string& fieldName) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur) return std::nullopt;

    std::unordered_map<std::string, bool> seen; // cycle detection during base traversal

    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        auto fit = cur->fields.find(fieldName);
        if (fit != cur->fields.end()) return fit->second;

        if (!cur->base) break; // no parent
        cur = getClassPtr(*cur->base); // walk to base class
        if (!cur) break;
    }
    return std::nullopt;
}

std::optional<MethodInfo> Env::lookupMethod(const std::string& className, const std::string& methodName) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur) return std::nullopt;

    std::unordered_map<std::string, bool> seen; // cycle detection

    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        auto mit = cur->methods.find(methodName);
        if (mit != cur->methods.end()) return mit->second;

        if (!cur->base) break;
        cur = getClassPtr(*cur->base);
        if (!cur) break;
    }
    return std::nullopt;
}

// layout helpers: total (self + bases)
int Env::countAllFields(const std::string& className) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur) return 0;

    int total = 0;
    std::unordered_map<std::string, bool> seen; // cycle detection

    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        total += static_cast<int>(cur->fields.size());

        if (!cur->base) break;
        cur = getClassPtr(*cur->base);
        if (!cur) break;
    }
    return total;
}

int Env::countAllMethods(const std::string& className) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur) return 0;

    int total = 0;
    std::unordered_map<std::string, bool> seen; // cycle detection

    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        total += static_cast<int>(cur->methods.size());

        if (!cur->base) break;
        cur = getClassPtr(*cur->base);
        if (!cur) break;
    }
    return total;
}

// objects1 helpers: bases only (exclude self)
bool Env::hasFieldInBases(const std::string& className, const std::string& fieldName) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur || !cur->base) return false;

    std::unordered_map<std::string, bool> seen; // cycle detection

    cur = getClassPtr(*cur->base);
    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        if (cur->fields.count(fieldName)) return true;

        if (!cur->base) break;
        cur = getClassPtr(*cur->base);
        if (!cur) break;
    }
    return false;
}

bool Env::hasMethodInBases(const std::string& className, const std::string& methodName) const
{
    const ClassInfo* cur = getClassPtr(className);
    if (!cur || !cur->base) return false;

    std::unordered_map<std::string, bool> seen; // cycle detection

    cur = getClassPtr(*cur->base);
    while (cur)
    {
        if (seen[cur->name])
            throw LatteError("Cycle in inheritance involving: " + cur->name, 0);
        seen[cur->name] = true;

        if (cur->methods.count(methodName)) return true;

        if (!cur->base) break;
        cur = getClassPtr(*cur->base);
        if (!cur) break;
    }
    return false;
}
