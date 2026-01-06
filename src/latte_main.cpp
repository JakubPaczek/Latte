#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "frontend/Parser.H"
#include "frontend/Absyn.H"
#include "frontend/Printer.H"

#include "typecheck.hpp"
#include "latte_error.hpp"

#include "backend/ir.hpp"
#include "backend/regalloc.hpp"
#include "backend/x86_emit.hpp"
#include "backend/codegen.hpp"

ModuleIR buildModuleIR(Program* program);

using namespace std;

static std::string makeOutPath(const char* inPath)
{
    std::string s(inPath ? inPath : "");
    auto slash = s.find_last_of("/\\");
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    {
        return s + ".s";
    }
    s.replace(dot, std::string::npos, ".s");
    return s;
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

static void emitStringRodataAndHelpers(FILE* out, const ModuleIR& mod)
{
    // --- rodata
    std::fprintf(out, ".section .rodata\n");
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, ".LC%zu:\n", i);
        std::string esc = asmEscape(mod.stringLits[i]);
        std::fprintf(out, "  .asciz \"%s\"\n", esc.c_str());
    }

    // --- back to text
    std::fprintf(out, ".text\n");

    // --- string literal getters: char* __latte_str_N()
    for (size_t i = 0; i < mod.stringLits.size(); ++i)
    {
        std::fprintf(out, "__latte_str_%zu:\n", i);
        std::fprintf(out, "  movl $.LC%zu, %%eax\n", i);
        std::fprintf(out, "  ret\n");
    }

    // --- concat helper: char* __latte_concat(char* a, char* b)
    // cdecl: a = 8(%ebp), b = 12(%ebp)
    std::fputs(
        "__latte_concat:\n"
        "  pushl %ebp\n"
        "  movl %esp, %ebp\n"
        "  pushl %ebx\n"
        "  pushl %esi\n"
        "  pushl %edi\n"
        "  movl 8(%ebp), %esi\n"
        "  movl 12(%ebp), %edi\n"
        "  pushl %esi\n"
        "  call strlen\n"
        "  addl $4, %esp\n"
        "  movl %eax, %ebx\n"
        "  pushl %edi\n"
        "  call strlen\n"
        "  addl $4, %esp\n"
        "  movl %eax, %ecx\n"
        "  leal 1(%ebx,%ecx), %eax\n"
        "  pushl %eax\n"
        "  call malloc\n"
        "  addl $4, %esp\n"
        "  movl %eax, %edx\n"
        "  pushl %ebx\n"
        "  pushl %esi\n"
        "  pushl %edx\n"
        "  call memcpy\n"
        "  addl $12, %esp\n"
        "  leal (%edx,%ebx), %eax\n"
        "  leal 1(%ecx), %ecx\n"
        "  pushl %ecx\n"
        "  pushl %edi\n"
        "  pushl %eax\n"
        "  call memcpy\n"
        "  addl $12, %esp\n"
        "  movl %edx, %eax\n"
        "  popl %edi\n"
        "  popl %esi\n"
        "  popl %ebx\n"
        "  popl %ebp\n"
        "  ret\n",
        out
    );

}

static std::string makeExePath(const char* inPath)
{
    std::string s(inPath ? inPath : "");
    auto slash = s.find_last_of("/\\");
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return s;                  // brak rozszerzenia
    return s.substr(0, dot);       // utnij ".lat"
}

static bool isLatcX86Driver(const char* argv0)
{
    std::string s(argv0 ? argv0 : "");
    // weź basename (po / lub \)
    auto slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    return s == "latc_x86" || s == "latc_x86.exe";
}

static void linkX86(const std::string& asmPath, const std::string& exePath)
{
#ifdef _WIN32
    (void)asmPath;
    (void)exePath;
    return;
#else
    // -no-pie: żeby nie robić PIE i nie dostawać DT_TEXTREL warningów
    // >link.log 2>&1: żeby nie psuć "OK" jako pierwszej linii stderr
    std::string log = exePath + ".link.log";
    std::string cmd =
        "gcc -m32 -no-pie -o \"" + exePath + "\" \"" + asmPath + "\" lib/runtime.o"
        " >\"" + log + "\" 2>&1";

    int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw std::runtime_error("Link failed. See: " + log);
#endif
}



int main(int argc, char* argv[])
{
    // no file name in args
    if (argc != 2)
    {
        cerr << "ERROR\n";
        cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    }


    // C FILE for bnfc
    const char* filename = argv[1];
    FILE* input = std::fopen(filename, "r");
    if (!input)
    {
        std::cerr << "ERROR\n";
        std::perror("fopen");
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

        // end of frontend


        // BACKEND: IR -> RA -> x86
        ModuleIR mod = buildModuleIR(program);

        const std::string outPath = makeOutPath(filename);
        FILE* out = std::fopen(outPath.c_str(), "w");
        if (!out)
        {
            std::perror("fopen");
            throw std::runtime_error("Cannot open output file: " + outPath);
        }

        // nagłówki
        std::fprintf(out, ".text\n");

        // Latte runtime (u Ciebie mogą być w runtime.o, ale externy nie zaszkodzą)
        std::fprintf(out, ".extern printInt\n");
        std::fprintf(out, ".extern printString\n");
        std::fprintf(out, ".extern error\n");
        std::fprintf(out, ".extern readInt\n");
        std::fprintf(out, ".extern readString\n");

        // libc / libgcc, których używamy w helperach / codegenie
        std::fprintf(out, ".extern strlen\n");
        std::fprintf(out, ".extern malloc\n");
        std::fprintf(out, ".extern memcpy\n");
        std::fprintf(out, ".extern strcmp\n");
        std::fprintf(out, ".extern __divsi3\n");
        std::fprintf(out, ".extern __modsi3\n");

        // TU: wypisz .rodata + helpery zanim zaczniesz emitować funkcje programu
        emitStringRodataAndHelpers(out, mod);

        // potem zwykły emit funkcji
        RegAllocator ra;
        X86Emitter emit;

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

        // jeśli to latc_x86: utwórz też executable obok .s
        if (isLatcX86Driver(argv[0]))
        {
            const std::string exePath = makeExePath(filename);
            linkX86(outPath, exePath);
        }

        std::cerr << "OK\n";

        std::cerr << "Wrote asm: " << outPath << "\n";
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


    return 0;
}
