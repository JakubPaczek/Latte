#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>

#include "frontend/Parser.H"
#include "frontend/Absyn.H"

#include "typecheck.hpp"
#include "latte_error.hpp"

#include "backend/ir.hpp"
#include "backend/regalloc.hpp"
#include "backend/x86_emit.hpp"
#include "backend/codegen.hpp"

ModuleIR buildModuleIR(Program* program);

static std::string makeOutPath(const char* inPath)
{
    std::string s(inPath ? inPath : "");
    auto slash = s.find_last_of("/\\");
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return s + ".s";
    s.replace(dot, std::string::npos, ".s");
    return s;
}

static std::string makeExePath(const char* inPath)
{
    std::string s(inPath ? inPath : "");
    auto slash = s.find_last_of("/\\");
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return s;
    return s.substr(0, dot);
}

static bool isLatcX64Driver(const char* argv0)
{
    std::string s(argv0 ? argv0 : "");
    auto slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    return s == "latc_x86_64" || s == "latc_x86_64.exe";
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

static void emitStringRodataAndHelpersX64(FILE* out, const ModuleIR& mod)
{
    // .rodata for string literals
    std::fprintf(out, ".section .rodata\n");
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, ".LC%zu:\n", i);
        std::string esc = asmEscape(mod.stringLits[i]);
        std::fprintf(out, "  .asciz \"%s\"\n", esc.c_str());
    }

    // back to text
    std::fprintf(out, ".text\n");

    // char* __latte_str_N()
    // SysV x86_64: return in %rax, address via RIP-relative LEA.
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, ".globl __latte_str_%zu\n", i);
        std::fprintf(out, "__latte_str_%zu:\n", i);
        std::fprintf(out, "  leaq .LC%zu(%%rip), %%rax\n", i);
        std::fprintf(out, "  ret\n");
    }

    // IMPORTANT:
    // Do NOT emit __latte_concat here in asm for x64.
    // Provide it in lib/runtime.c and compile into lib/runtime.o.
}

static void linkX64(const std::string& asmPath, const std::string& exePath)
{
#ifdef _WIN32
    // MinGW gcc can assemble+link .s directly. // Windows linking
    std::string log = exePath + ".link.log";

    // -no-pie is Linux-specific; omit on Windows.
    std::string cmd =
        "gcc -o \"" + exePath + "\" \"" + asmPath + "\" lib/runtime.o -lgcc"
        " >\"" + log + "\" 2>&1";


    int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw std::runtime_error("Link failed. See: " + log);
#else
    std::string log = exePath + ".link.log";
    std::string cmd =
        "gcc -no-pie -o \"" + exePath + "\" \"" + asmPath + "\" lib/runtime.o"
        " >\"" + log + "\" 2>&1";

    int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw std::runtime_error("Link failed. See: " + log);
#endif
}

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
        // FRONTEND
        TypeChecker checker;
        checker.checkProgram(program);

        // BACKEND: AST -> IR
        ModuleIR mod = buildModuleIR(program);

        // Output asm path
        const std::string outPath = makeOutPath(filename);
        FILE* out = std::fopen(outPath.c_str(), "w");
        if (!out)
        {
            std::perror("fopen");
            throw std::runtime_error("Cannot open output file: " + outPath);
        }

        // text section
        std::fprintf(out, ".text\n");

        // Latte runtime functions (provided by lib/runtime.o)
        std::fprintf(out, ".extern printInt\n");
        std::fprintf(out, ".extern printString\n");
        std::fprintf(out, ".extern error\n");
        std::fprintf(out, ".extern readInt\n");
        std::fprintf(out, ".extern readString\n");

        // string concat helper (NOW IN runtime.c)
        std::fprintf(out, ".extern __latte_concat\n");

        // if you rely on strcmp somewhere (string compare), keep it:
        std::fprintf(out, ".extern strcmp\n");

        // Emit .rodata + string getters
        emitStringRodataAndHelpersX64(out, mod);

        // Emit functions
        RegAllocator ra;
        X86Emitter emit; // you'll update this emitter to x86_64

        for (const auto& fn : mod.funs)
        {
            AllocResult alloc = ra.allocate(fn);
            emit.emitFunction(out, fn, alloc);
        }

        if (std::fclose(out) != 0)
        {
            std::perror("fclose");
            throw std::runtime_error("Error closing output file: " + outPath);
        }

        // Link executable only for latc_x86_64 driver.
        if (isLatcX64Driver(argv[0]))
        {
            const std::string exePath = makeExePath(filename);
            linkX64(outPath, exePath);
        }

        std::cerr << "OK\n";
        std::cerr << "Wrote asm: " << outPath << "\n";
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
