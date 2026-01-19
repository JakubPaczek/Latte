#include <cstdio>
#include <cstdlib>   // std::system
#include <iostream>
#include <stdexcept>
#include <string>

#include "frontend/Parser.H"
#include "frontend/Absyn.H"

#include "semantic/typecheck.h"
#include "semantic/latte_error.h"

#include "backend/ir.h"
#include "backend/regalloc.h"
#include "backend/x86_emit.h"
#include "backend/codegen.h"

// AST -> IR entry (your codegen layer).
ModuleIR buildModuleIR(Program* program);

static Program* parseProgramOrThrow(const char* path)
{
    FILE* in = std::fopen(path, "r");
    if (!in)
        throw std::runtime_error("Cannot open input file");

    Program* program = pProgram(in);
    std::fclose(in);

    if (!program)
        throw std::runtime_error("Parse error");

    return program;
}

static std::string replaceExtWithS(const std::string& inPath)
{
    // Replace last ".xxx" with ".s". If no extension, append ".s".
    const auto slash = inPath.find_last_of('/');
    const auto dot   = inPath.find_last_of('.');
    const bool hasExt = (dot != std::string::npos) && (slash == std::string::npos || dot > slash);

    if (!hasExt) return inPath + ".s";

    std::string out = inPath;
    out.replace(dot, std::string::npos, ".s");
    return out;
}

static std::string dropExt(const std::string& inPath)
{
    // Remove last ".xxx". If no extension, return original.
    const auto slash = inPath.find_last_of('/');
    const auto dot   = inPath.find_last_of('.');
    const bool hasExt = (dot != std::string::npos) && (slash == std::string::npos || dot > slash);

    if (!hasExt) return inPath;
    return inPath.substr(0, dot);
}

static std::string asmEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);

    for (unsigned char c : s)
    {
        switch (c)
        {
        case '\\': o += "\\\\"; break;
        case '\"': o += "\\\""; break;
        case '\n': o += "\\n";  break;
        case '\t': o += "\\t";  break;
        case '\r': o += "\\r";  break;
        default:
            if (c < 32)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\%03o", (unsigned)c);
                o += buf;
            }
            else
            {
                o += char(c);
            }
        }
    }

    return o;
}

static void emitStringRodataX64(FILE* out, const ModuleIR& mod)
{
    // String literals in .rodata.
    std::fprintf(out, ".section .rodata\n");
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, ".LC%zu:\n", i);
        const std::string esc = asmEscape(mod.stringLits[i]);
        std::fprintf(out, "  .asciz \"%s\"\n", esc.c_str());
    }
    std::fprintf(out, ".text\n");
}

static void emitStringGettersX64(FILE* out, const ModuleIR& mod)
{
    // char* __latte_str_N() returns address of .LCN in %rax (SysV ABI).
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, ".globl __latte_str_%zu\n", i);
        std::fprintf(out, "__latte_str_%zu:\n", i);
        std::fprintf(out, "  leaq .LC%zu(%%rip), %%rax\n", i);
        std::fprintf(out, "  ret\n");
    }
}

static void linkExecutableX64(const std::string& asmPath, const std::string& exePath)
{
    // Assemble+link in one step; save errors to a log file.
    const std::string logPath = exePath + ".link.log";

    const std::string cmd =
        "gcc -no-pie -o \"" + exePath + "\" \"" + asmPath + "\" lib/runtime.o"
        " >\"" + logPath + "\" 2>&1";

    const int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw std::runtime_error("Link failed (see " + logPath + ")");
}

int main(int argc, char* argv[])
{
    if (argc != 2) // input file path only
    {
        std::cerr << "ERROR\n";
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    }

    const char* path = argv[1];

    try
    {
        // Parse AST.
        Program* program = parseProgramOrThrow(path);

        // Semantic checks.
        TypeChecker checker;
        checker.checkProgram(program);

        // AST -> IR.
        ModuleIR mod = buildModuleIR(program);

        // Output paths.
        const std::string inPath(path);
        const std::string asmPath = replaceExtWithS(inPath);
        const std::string exePath = dropExt(inPath);

        // Open output file.
        FILE* out = std::fopen(asmPath.c_str(), "w");
        if (!out)
            throw std::runtime_error("Cannot open output .s file");

        // External runtime symbols.
        std::fprintf(out, ".text\n");
        std::fprintf(out, ".extern printInt\n");
        std::fprintf(out, ".extern printString\n");
        std::fprintf(out, ".extern readInt\n");
        std::fprintf(out, ".extern readString\n");
        std::fprintf(out, ".extern error\n");
        std::fprintf(out, ".extern __latte_concat\n");
        std::fprintf(out, ".extern strcmp\n"); // if used for string relops

        // Strings.
        emitStringRodataX64(out, mod);
        emitStringGettersX64(out, mod);

        // Functions.
        RegAllocator ra;
        X86Emitter emitter; // expected to be x86_64 SysV
        for (const auto& fn : mod.funs)
        {
            AllocResult alloc = ra.allocate(fn);
            emitter.emitFunction(out, fn, alloc);
        }

        if (std::fclose(out) != 0)
            throw std::runtime_error("Error closing output .s file");

        // Produce executable.
        linkExecutableX64(asmPath, exePath);

        std::cerr << "OK\n";
        return 0;
    }
    catch (const LatteError& e) // expected errors with line info
    {
        std::cerr << "ERROR\n";
        if (e.line() > 0) std::cerr << "Line " << e.line() << ": ";
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e) // parse / IO / internal errors
    {
        std::cerr << "ERROR\n";
        std::cerr << e.what() << "\n";
        return 1;
    }
}
