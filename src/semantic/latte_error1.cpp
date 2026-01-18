#include "latte_error.h"

LatteError::LatteError(const std::string& message, int line)
    : std::runtime_error(message)
    , line_(line)
{
}

int LatteError::line() const noexcept
{
    return line_;
}
