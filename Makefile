# Kompilatory i flagi
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g
UNAME_S := $(shell uname -s)
ARCH32 :=
ifeq ($(UNAME_S),Linux)
  ARCH32 := -m32
endif

# Ścieżki
SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
LIB_DIR      := lib

# Programy w root (wymagania)
TARGET      := latc
TARGET_X86  := latc_x86

# Runtime (wymagania: lib/runtime.o + źródło)
RUNTIME_SRC := $(LIB_DIR)/runtime.c
RUNTIME_OBJ := $(LIB_DIR)/runtime.o

# Obiekty z BNFC (powstają po "make -C src/frontend")
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

# Twoje obiekty (main + semantyka)
CORE_OBJS := \
  $(SRC_DIR)/latte_main.o    \
  $(SRC_DIR)/typecheck.o     \
  $(SRC_DIR)/env.o           \
  $(SRC_DIR)/latte_error.o

# Backend
BACKEND_OBJS := \
  $(BACKEND_DIR)/codegen.o   \
  $(BACKEND_DIR)/regalloc.o  \
  $(BACKEND_DIR)/x86_emit.o

.PHONY: all clean distclean frontend

ifeq ($(UNAME_S),Linux)
all: $(TARGET) $(TARGET_X86) $(RUNTIME_OBJ)
else
all: $(TARGET) $(TARGET_X86)
endif

# ============================================================
# Frontend (BNFC + parser/lexer)
# ============================================================

frontend:
	$(MAKE) -C $(FRONTEND_DIR)

# ============================================================
# Runtime
# ============================================================

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@mkdir -p $(LIB_DIR)
	$(CC) $(CFLAGS) $(ARCH32) -c $< -o $@

# ============================================================
# Kompilacja plików C++
# ============================================================

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
  $(FRONTEND_DIR)/Absyn.H $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.hpp \
  $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.hpp \
  $(BACKEND_DIR)/ir.hpp $(BACKEND_DIR)/regalloc.hpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# ============================================================
# Linkowanie kompilatora (host)
# ============================================================

$(TARGET): frontend $(CORE_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -o $@ \
	  $(CORE_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

# latc_x86 jako kopia latc (najprościej na teraz)
$(TARGET_X86): $(TARGET)
	cp -f $(TARGET) $(TARGET_X86)

# ============================================================
# Sprzątanie
# ============================================================

clean:
	rm -f $(TARGET) $(TARGET_X86) $(SRC_DIR)/*.o $(BACKEND_DIR)/*.o $(RUNTIME_OBJ)
	$(MAKE) -C $(FRONTEND_DIR) clean || true

distclean: clean
	rm -rf $(FRONTEND_DIR)
