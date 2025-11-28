#include "env.hpp"

Env::Env()
{
    // global scope for variables
    pushScope();
}

void Env::enterFunction(const std::string& name, const FunInfo& info)
{
    globalFunctions_[name] = info;
}

std::optional<FunInfo> Env::lookupFunction(const std::string& name) const
{
    auto it = globalFunctions_.find(name);
    if (it == globalFunctions_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void Env::pushScope()
{
    scopes_.emplace_back();
}

void Env::popScope()
{
    if (!scopes_.empty())
    {
        scopes_.pop_back();
    }
}

void Env::declareVar(const std::string& name, const VarInfo& info)
{
    if (scopes_.empty())
    {
        pushScope();
    }
    scopes_.back()[name] = info;
}

std::optional<VarInfo> Env::lookupVar(const std::string& name) const
{
    // begin search from the most inside scope
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
        {
            return found->second;
        }
    }
    return std::nullopt;
}

bool Env::isVarDeclaredInCurrentScope(const std::string& name) const
{
    if (scopes_.empty())
    {
        return false;
    }
    const auto& current = scopes_.back();
    return current.find(name) != current.end();
}
