# Top-level Makefile that:
# 1) ALWAYS regenerates BNFC C++ frontend from .cf
# 2) Then builds the generated frontend (Parser/Lexer/Absyn/Printer)
# 3) Then builds your compiler + runtime

# -----------------------------
# Tools / flags
# -----------------------------
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

BNFC     := bnfc
BNFCFLAGS:= --cpp

# -----------------------------
# Layout
# -----------------------------
SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
SEM_DIR      := $(SRC_DIR)/semantic
LIB_DIR      := lib

# Path to your grammar (.cf)
# CHANGE THIS if your file name/location is different:
GRAMMAR := LatteCPP.cf
# e.g. GRAMMAR := $(FRONTEND_DIR)/LatteCPP.cf

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
  NULLDEV   := NUL
  EXEEXT    := .exe
  TARGET_WIN        := $(TARGET)$(EXEEXT)
  TARGET_X86_64_WIN := $(TARGET_X86_64)$(EXEEXT)
else
  RM        := rm -f
  RMDIR     := rm -rf
  COPY      := cp -f
  MKDIR_P   := mkdir -p $(LIB_DIR)
  NULLDEV   := /dev/null
  EXEEXT    :=
  TARGET_WIN        := $(TARGET)
  TARGET_X86_64_WIN := $(TARGET_X86_64)
endif

# -----------------------------
# Phony
# -----------------------------
.PHONY: all clean distclean bnfc frontend runtime

all: frontend $(TARGET_WIN) $(TARGET_X86_64_WIN) runtime

# -----------------------------
# 1) ALWAYS regenerate BNFC frontend from .cf
# -----------------------------
bnfc:
	@echo "==> BNFC: regenerating C++ frontend from $(GRAMMAR) into $(FRONTEND_DIR)"
	$(BNFC) $(BNFCFLAGS) -o $(FRONTEND_DIR) $(GRAMMAR)

# -----------------------------
# 2) Build generated frontend
#    (We run bnfc EVERY TIME you call make, as requested.)
# -----------------------------
frontend: bnfc
	@echo "==> Building frontend in $(FRONTEND_DIR)"
	$(MAKE) -C $(FRONTEND_DIR)

# -----------------------------
# 3) runtime.o
# -----------------------------
runtime: $(RUNTIME_OBJ)

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# Compiler objects
# Adjust these lists to your actual filenames.
# (I’m keeping your structure; change semantic/*.o paths if needed.)
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

# Frontend objects are produced by $(MAKE) -C src/frontend, but the
# link step needs them. Their names depend on BNFC output; these are typical:
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o \
  $(FRONTEND_DIR)/Buffer.o \
  $(FRONTEND_DIR)/Parser.o \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

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
	-$(RM) $(RUNTIME_OBJ) 2>$(NULLDEV) || exit 0
	$(MAKE) -C $(FRONTEND_DIR) clean || exit 0
else
	$(RM) $(TARGET_WIN) $(TARGET_X86_64_WIN) \
	      $(SRC_DIR)/*.o $(SEM_DIR)/*.o $(BACKEND_DIR)/*.o $(RUNTIME_OBJ)
	$(MAKE) -C $(FRONTEND_DIR) clean || true
endif

# distclean also removes BNFC-generated frontend sources,
# so the next build will fully regenerate everything.
distclean: clean
ifeq ($(OS),Windows_NT)
	-$(RMDIR) $(FRONTEND_DIR) 2>$(NULLDEV) || exit 0
else
	$(RMDIR) $(FRONTEND_DIR)
endif
