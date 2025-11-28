#pragma once

#include <stdexcept>
#include <string>

class LatteError : public std::runtime_error {
public:
    LatteError(const std::string& message, int line);

    int line() const noexcept;

private:
    int line_;
};
