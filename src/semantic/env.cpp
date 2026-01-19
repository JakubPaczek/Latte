#include "env.h"
#include <stdexcept>

Env::Env()
{
    // global scope for variables
    pushScope();
}

// functions
void Env::enterFunction(const std::string& name, const FunInfo& info)
{
    globalFunctions_[name] = info;
}

bool Env::tryEnterFunction(const std::string& name, const FunInfo& info)
{
    return globalFunctions_.emplace(name, info).second;
}

std::optional<FunInfo> Env::lookupFunction(const std::string& name) const
{
    auto it = globalFunctions_.find(name);
    if (it == globalFunctions_.end())
        return std::nullopt;
    return it->second;
}

//scopes
void Env::pushScope()
{
    scopes_.emplace_back();
}

void Env::popScope()
{
    if (!scopes_.empty())
        scopes_.pop_back();
}

void Env::declareVar(const std::string& name, const VarInfo& info)
{
    if (scopes_.empty())
        pushScope();
    scopes_.back()[name] = info;
}

bool Env::tryDeclareVar(const std::string& name, const VarInfo& info)
{
    if (scopes_.empty())
        pushScope();
    auto& cur = scopes_.back();
    return cur.emplace(name, info).second;
}

std::optional<VarInfo> Env::lookupVar(const std::string& name) const
{
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
            return found->second;
    }
    return std::nullopt;
}

bool Env::isVarDeclaredInCurrentScope(const std::string& name) const
{
    if (scopes_.empty())
        return false;
    const auto& current = scopes_.back();
    return current.find(name) != current.end();
}

// classess
bool Env::tryEnterClass(const ClassInfo& c)
{
    return classes_.emplace(c.name, c).second;
}

std::optional<ClassInfo> Env::lookupClass(const std::string& name) const
{
    auto it = classes_.find(name);
    if (it == classes_.end())
        return std::nullopt;
    return it->second;
}

std::optional<FieldInfo> Env::lookupField(const std::string& className, const std::string& fieldName) const
{
    auto it = classes_.find(className);
    if (it == classes_.end()) return std::nullopt;

    const ClassInfo* cur = &it->second;
    while (cur)
    {
        auto fit = cur->fields.find(fieldName);
        if (fit != cur->fields.end()) return fit->second;

        if (!cur->base) break;
        auto bit = classes_.find(*cur->base);
        if (bit == classes_.end()) break;
        cur = &bit->second;
    }
    return std::nullopt;
}

std::optional<MethodInfo> Env::lookupMethod(const std::string& className, const std::string& methodName) const
{
    auto it = classes_.find(className);
    if (it == classes_.end()) return std::nullopt;

    const ClassInfo* cur = &it->second;
    while (cur)
    {
        auto mit = cur->methods.find(methodName);
        if (mit != cur->methods.end()) return mit->second;

        if (!cur->base) break;
        auto bit = classes_.find(*cur->base);
        if (bit == classes_.end()) break;
        cur = &bit->second;
    }
    return std::nullopt;
}

ClassInfo& Env::getClassRef(const std::string& name)
{
    auto it = classes_.find(name);
    if (it == classes_.end()) throw std::runtime_error("Unknown class: " + name);
    return it->second;
}