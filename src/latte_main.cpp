#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "frontend/Parser.H"
#include "frontend/Absyn.H"
#include "frontend/Printer.H"

#include "typecheck.hpp"
#include "latte_error.hpp"

static void printUsage(const char* progName)
{
    std::cerr << "Usage: " << progName << " <source-file>\n";
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "ERROR\n";
        printUsage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    FILE* input = std::fopen(filename, "r");
    if (!input)
    {
        std::perror("fopen");
        std::cerr << "ERROR\n";
        return 1;
    }

    // parsing
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
        // semantic analysis
        TypeChecker checker;
        checker.checkProgram(program);

        // in case of success:
        std::cerr << "OK\n";

        // end of frontend

        return 0;
    }
    catch (const LatteError& e)
    {
        std::cerr << "ERROR\n";
        if (e.line() > 0)
        {
            std::cerr << "Line " << e.line() << ": ";
        }
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        // other exceptions
        std::cerr << "ERROR\n";
        std::cerr << "Internal error: " << e.what() << "\n";
        return 1;
    }
}
