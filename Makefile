# -----------------------------
# Tools / flags
# -----------------------------
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

BNFC      := bnfc
BNFCFLAGS := -m --cpp

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
# Frontend objects (built by make -C src/frontend)
# -----------------------------
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o \
  $(FRONTEND_DIR)/Buffer.o \
  $(FRONTEND_DIR)/Parser.o \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

# -----------------------------
# Compiler objects
# -----------------------------
CORE_OBJS := \
  $(SRC_DIR)/latc.o

CORE_X86_64_OBJS := \
  $(SRC_DIR)/latc_x86_64.o

SEM_OBJS := \
  $(SEM_DIR)/typecheck.o \
  $(SEM_DIR)/env.o \
  $(SEM_DIR)/latte_error.o

BACKEND_OBJS := \
  $(BACKEND_DIR)/codegen.o \
  $(BACKEND_DIR)/regalloc.o \
  $(BACKEND_DIR)/x86_emit.o

.PHONY: all clean distclean frontend runtime bnfc

all: frontend runtime $(TARGET) $(TARGET_X86_64)

# -----------------------------
# BNFC (run manually when .cf changes)
# -----------------------------
bnfc:
	@echo "==> BNFC: regenerating C++ frontend from $(GRAMMAR) into $(FRONTEND_DIR)"
	$(BNFC) $(BNFCFLAGS) -o $(FRONTEND_DIR) $(GRAMMAR)

# -----------------------------
# Build frontend (expects BNFC already generated src/frontend/Makefile)
# -----------------------------
frontend:
	@if [ ! -f "$(FRONTEND_DIR)/Makefile" ]; then \
	  echo "ERROR: $(FRONTEND_DIR)/Makefile not found."; \
	  echo "Run: make bnfc"; \
	  exit 1; \
	fi
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
$(SRC_DIR)/latc.o: $(SRC_DIR)/latc.cpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -I$(BACKEND_DIR) -c $< -o $@

$(SRC_DIR)/latc_x86_64.o: $(SRC_DIR)/latc_x86_64.cpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -I$(BACKEND_DIR) -c $< -o $@

$(SEM_DIR)/typecheck.o: $(SEM_DIR)/typecheck.cpp $(SEM_DIR)/typecheck.h
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/env.o: $(SEM_DIR)/env.cpp $(SEM_DIR)/env.h
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/latte_error.o: $(SEM_DIR)/latte_error.cpp $(SEM_DIR)/latte_error.h
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

# -----------------------------
# Backend objects
# -----------------------------
$(BACKEND_DIR)/codegen.o: $(BACKEND_DIR)/codegen.cpp $(BACKEND_DIR)/codegen.h \
  $(BACKEND_DIR)/ir.h
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -I$(BACKEND_DIR) -c $< -o $@

$(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.h \
  $(BACKEND_DIR)/ir.h
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -I$(BACKEND_DIR) -c $< -o $@

$(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.h \
  $(BACKEND_DIR)/ir.h $(BACKEND_DIR)/regalloc.h
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -I$(BACKEND_DIR) -c $< -o $@

# -----------------------------
# Link compiler
# -----------------------------
$(TARGET): frontend $(CORE_OBJS) $(SEM_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -I$(BACKEND_DIR) -o $@ \
	  $(CORE_OBJS) $(SEM_OBJS) $(FRONTEND_OBJS)

$(TARGET_X86_64): frontend runtime $(CORE_X86_64_OBJS) $(SEM_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -I$(BACKEND_DIR) -o $@ \
	  $(CORE_X86_64_OBJS) $(SEM_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

# -----------------------------
# Cleanup
# -----------------------------
clean:
	rm -f $(TARGET) $(TARGET_X86_64) \
	      $(SRC_DIR)/*.o $(SEM_DIR)/*.o $(BACKEND_DIR)/*.o $(RUNTIME_OBJ)
	$(MAKE) -C $(FRONTEND_DIR) clean || true
	@find lattests -type f ! -name '*.lat' ! -name '*.output' ! -name '*.input' -delete

distclean: clean
	rm -rf $(FRONTEND_DIR)
