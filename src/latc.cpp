#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>

#include "frontend/Parser.H"
#include "frontend/Absyn.H"

#include "semantic/typecheck.h"
#include "semantic/latte_error.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "ERROR\n";
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    }

    const char* filename = argv[1];
    FILE* input = std::fopen(filename, "r");
    if (!input)
    {
        std::cerr << "ERROR\n";
        std::perror("fopen");
        return 1;
    }

    Program* program = pProgram(input);
    std::fclose(input);

    if (!program)
    {
        std::cerr << "ERROR\n";
        std::cerr << "Parse error in file " << filename << "\n";
        return 1;
    }

    try
    {
        // frontend
        TypeChecker checker;
        checker.checkProgram(program);

        std::cerr << "OK\n";
        return 0;
    }
    catch (const LatteError& e)
    {
        std::cerr << "ERROR\n";
        if (e.line() > 0) std::cerr << "Line " << e.line() << ": ";
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR\n";
        std::cerr << "Internal error: " << e.what() << "\n";
        return 1;
    }
}
