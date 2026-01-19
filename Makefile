# Top-level Makefile:
# - ALWAYS regenerates BNFC frontend from .cf
# - then runs flex+bison manually
# - then compiles ONLY needed frontend objects (no TestLatteCPP)
# - then builds your compiler + runtime

# -----------------------------
# Tools / flags
# -----------------------------
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

BNFC     := bnfc
BNFCFLAGS:= --cpp

FLEX     := flex
BISON    := bison

# prefixes must match what BNFC-generated frontend expects
FLEX_PREFIX  := Latte_cpp_
BISON_PREFIX := latte_cpp_

# -----------------------------
# Layout
# -----------------------------
SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
SEM_DIR      := $(SRC_DIR)/semantic
LIB_DIR      := lib

GRAMMAR := $(SRC_DIR)/LatteCPP.cf

# -----------------------------
# Targets
# -----------------------------
TARGET        := latc
TARGET_X86_64 := latc_x86_64

RUNTIME_SRC := $(LIB_DIR)/runtime.c
RUNTIME_OBJ := $(LIB_DIR)/runtime.o

# -----------------------------
# Platform helpers (Windows vs Unix)
# -----------------------------
ifeq ($(OS),Windows_NT)
  RM        := del /Q
  RMDIR     := rmdir /S /Q
  COPY      := copy /Y
  MKDIR_P   := if not exist "$(LIB_DIR)" mkdir "$(LIB_DIR)"
  MKDIR_FE  := if not exist "$(FRONTEND_DIR)" mkdir "$(FRONTEND_DIR)"
  NULLDEV   := NUL
  EXEEXT    := .exe
  TARGET_WIN        := $(TARGET)$(EXEEXT)
  TARGET_X86_64_WIN := $(TARGET_X86_64)$(EXEEXT)
else
  RM        := rm -f
  RMDIR     := rm -rf
  COPY      := cp -f
  MKDIR_P   := mkdir -p $(LIB_DIR)
  MKDIR_FE  := mkdir -p $(FRONTEND_DIR)
  NULLDEV   := /dev/null
  EXEEXT    :=
  TARGET_WIN        := $(TARGET)
  TARGET_X86_64_WIN := $(TARGET_X86_64)
endif

# -----------------------------
# BNFC prefix/link fix (compile-time mapping)
# Your nm showed: latte_cpp__scan_string
# so map yy_scan_string/yy_delete_buffer -> latte_cpp__*
# -----------------------------
FRONTEND_FIX_DEFS := \
  -Dyylval=latte_cpp_lval \
  -Dyytext=latte_cpp_text \
  -Dyy_scan_string=latte_cpp__scan_string \
  -Dyy_delete_buffer=latte_cpp__delete_buffer

FRONTEND_CXXFLAGS := $(CXXFLAGS) $(FRONTEND_FIX_DEFS) -I$(FRONTEND_DIR)

# -----------------------------
# Phony
# -----------------------------
.PHONY: all clean distclean bnfc frontend runtime

all: bnfc frontend $(TARGET_WIN) $(TARGET_X86_64_WIN) runtime

# -----------------------------
# 1) ALWAYS regenerate BNFC frontend from .cf
# -----------------------------
bnfc:
	@echo "==> BNFC: regenerating C++ frontend from $(GRAMMAR) into $(FRONTEND_DIR)"
	@$(MKDIR_FE)
	$(BNFC) $(BNFCFLAGS) -o $(FRONTEND_DIR) $(GRAMMAR)

# -----------------------------
# 2) Generate Lexer.C and Parser.C (do NOT use BNFC-generated Makefile)
# -----------------------------
$(FRONTEND_DIR)/Lexer.C: bnfc $(FRONTEND_DIR)/LatteCPP.l
	@echo "==> FLEX: generating Lexer.C"
	$(FLEX) -P$(FLEX_PREFIX) -o$@ $(FRONTEND_DIR)/LatteCPP.l

$(FRONTEND_DIR)/Parser.C: bnfc $(FRONTEND_DIR)/LatteCPP.y
	@echo "==> BISON: generating Parser.C"
	$(BISON) -t -p$(BISON_PREFIX) $(FRONTEND_DIR)/LatteCPP.y -o $@

# -----------------------------
# 3) Compile frontend objects (NO Test.C, NO linking TestLatteCPP)
# -----------------------------
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o \
  $(FRONTEND_DIR)/Buffer.o \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Parser.o \
  $(FRONTEND_DIR)/Lexer.o

$(FRONTEND_DIR)/Absyn.o: bnfc $(FRONTEND_DIR)/Absyn.C $(FRONTEND_DIR)/Absyn.H
	$(CXX) $(FRONTEND_CXXFLAGS) -c $(FRONTEND_DIR)/Absyn.C -o $@

$(FRONTEND_DIR)/Buffer.o: bnfc $(FRONTEND_DIR)/Buffer.C $(FRONTEND_DIR)/Buffer.H
	$(CXX) $(FRONTEND_CXXFLAGS) -c $(FRONTEND_DIR)/Buffer.C -o $@

$(FRONTEND_DIR)/Printer.o: bnfc $(FRONTEND_DIR)/Printer.C $(FRONTEND_DIR)/Printer.H
	$(CXX) $(FRONTEND_CXXFLAGS) -c $(FRONTEND_DIR)/Printer.C -o $@

$(FRONTEND_DIR)/Parser.o: $(FRONTEND_DIR)/Parser.C $(FRONTEND_DIR)/Parser.H
	$(CXX) $(FRONTEND_CXXFLAGS) -c $(FRONTEND_DIR)/Parser.C -o $@

$(FRONTEND_DIR)/Lexer.o: $(FRONTEND_DIR)/Lexer.C
	$(CXX) $(FRONTEND_CXXFLAGS) -c $(FRONTEND_DIR)/Lexer.C -o $@

frontend: $(FRONTEND_OBJS)
	@echo "==> Frontend objects built (skipped TestLatteCPP)"

# -----------------------------
# 4) runtime.o
# -----------------------------
runtime: $(RUNTIME_OBJ)

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# Compiler objects
# -----------------------------
CORE_OBJS := \
  $(SRC_DIR)/latc.o

SEM_OBJS := \
  $(SEM_DIR)/typecheck.o \
  $(SEM_DIR)/env.o \
  $(SEM_DIR)/latte_error.o

BACKEND_OBJS := \
  $(BACKEND_DIR)/codegen.o \
  $(BACKEND_DIR)/regalloc.o \
  $(BACKEND_DIR)/x86_emit.o

# -----------------------------
# Compile C++ sources
# -----------------------------
$(SRC_DIR)/latc.o: $(SRC_DIR)/latc.cpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/typecheck.o: $(SEM_DIR)/typecheck.cpp $(SEM_DIR)/typecheck.h
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/env.o: $(SEM_DIR)/env.cpp $(SEM_DIR)/env.h
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/latte_error.o: $(SEM_DIR)/latte_error.cpp $(SEM_DIR)/latte_error.h
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

$(BACKEND_DIR)/codegen.o: $(BACKEND_DIR)/codegen.cpp $(BACKEND_DIR)/codegen.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# -----------------------------
# Link compiler
# -----------------------------
$(TARGET_WIN): frontend $(CORE_OBJS) $(SEM_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -o $@ \
	  $(CORE_OBJS) $(SEM_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

$(TARGET_X86_64_WIN): $(TARGET_WIN)
ifeq ($(OS),Windows_NT)
	$(COPY) $(TARGET_WIN) $(TARGET_X86_64_WIN) >$(NULLDEV)
else
	$(COPY) $(TARGET_WIN) $(TARGET_X86_64_WIN)
endif

# -----------------------------
# Cleanup
# -----------------------------
clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(TARGET_WIN) $(TARGET_X86_64_WIN) 2>$(NULLDEV) || exit 0
	-$(RM) $(SRC_DIR)\*.o 2>$(NULLDEV) || exit 0
	-$(RM) $(SEM_DIR)\*.o 2>$(NULLDEV) || exit 0
	-$(RM) $(BACKEND_DIR)\*.o 2>$(NULLDEV) || exit 0
	-$(RM) $(FRONTEND_DIR)\*.o 2>$(NULLDEV) || exit 0
	-$(RM) $(RUNTIME_OBJ) 2>$(NULLDEV) || exit 0
else
	$(RM) $(TARGET_WIN) $(TARGET_X86_64_WIN) \
	      $(SRC_DIR)/*.o $(SEM_DIR)/*.o $(BACKEND_DIR)/*.o $(FRONTEND_DIR)/*.o $(RUNTIME_OBJ)
endif

# distclean also removes BNFC-generated frontend sources,
# so the next build will fully regenerate everything.
distclean: clean
ifeq ($(OS),Windows_NT)
	-$(RMDIR) $(FRONTEND_DIR) 2>$(NULLDEV) || exit 0
else
	$(RMDIR) $(FRONTEND_DIR)
endif
