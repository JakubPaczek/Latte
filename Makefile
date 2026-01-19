# -----------------------------
# Tools / flags
# -----------------------------
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

# -----------------------------
# Layout
# -----------------------------
SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
LIB_DIR      := lib

TARGET        := latc
TARGET_X86_64 := latc_x86_64

RUNTIME_SRC := $(LIB_DIR)/runtime.c
RUNTIME_OBJ := $(LIB_DIR)/runtime.o

# -----------------------------
# Frontend objects produced by BNFC frontend Makefile
# (after: make -C src/frontend)
# -----------------------------
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

# -----------------------------
# Your compiler objects
# Dostosuj nazwy jeśli masz inne pliki
# -----------------------------
CORE_OBJS := \
  $(SRC_DIR)/latte_main.o \
  $(SRC_DIR)/typecheck.o \
  $(SRC_DIR)/env.o \
  $(SRC_DIR)/latte_error.o \
  $(BACKEND_DIR)/codegen.o \
  $(BACKEND_DIR)/regalloc.o \
  $(BACKEND_DIR)/x86_emit.o

.PHONY: all clean distclean frontend runtime

all: frontend runtime $(TARGET) $(TARGET_X86_64)

# -----------------------------
# Frontend (BNFC-generated makefile in src/frontend)
# -----------------------------
frontend:
	$(MAKE) -C $(FRONTEND_DIR)

# -----------------------------
# runtime.o
# -----------------------------
runtime: $(RUNTIME_OBJ)

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@mkdir -p $(LIB_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# Compile C++ sources
# -----------------------------
$(SRC_DIR)/latte_main.o: $(SRC_DIR)/latte_main.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@

$(SRC_DIR)/typecheck.o: $(SRC_DIR)/typecheck.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/env.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@

$(SRC_DIR)/env.o: $(SRC_DIR)/env.cpp $(SRC_DIR)/env.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(SRC_DIR)/latte_error.o: $(SRC_DIR)/latte_error.cpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/codegen.o: $(BACKEND_DIR)/codegen.cpp $(BACKEND_DIR)/codegen.hpp \
  $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.hpp \
  $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.hpp \
  $(BACKEND_DIR)/ir.hpp $(BACKEND_DIR)/regalloc.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# -----------------------------
# Link compiler
# -----------------------------
$(TARGET): frontend $(CORE_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -o $@ \
	  $(CORE_OBJS) $(FRONTEND_OBJS)

# latc_x86_64 is just a copy/alias of latc (driver name decides linking behaviour)
$(TARGET_X86_64): $(TARGET)
	cp -f $(TARGET) $(TARGET_X86_64)

# -----------------------------
# Cleanup
# -----------------------------
clean:
	rm -f $(TARGET) $(TARGET_X86_64) $(SRC_DIR)/*.o $(BACKEND_DIR)/*.o $(RUNTIME_OBJ)
	$(MAKE) -C $(FRONTEND_DIR) clean || true

# distclean: usuwa też frontend wygenerowany przez BNFC
distclean: clean
	rm -rf $(FRONTEND_DIR)
