CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
LIB_DIR      := lib

TARGET        := latc
TARGET_X86_64 := latc_x86_64

RUNTIME_SRC := $(LIB_DIR)/runtime.c
RUNTIME_OBJ := $(LIB_DIR)/runtime.o

FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

CORE_OBJS := \
  $(SRC_DIR)/latte_main.o    \
  $(SRC_DIR)/typecheck.o     \
  $(SRC_DIR)/env.o           \
  $(SRC_DIR)/latte_error.o

BACKEND_OBJS := \
  $(BACKEND_DIR)/codegen.o   \
  $(BACKEND_DIR)/regalloc.o  \
  $(BACKEND_DIR)/x86_emit.o

# ------------------------------------------------------------
# Platform helpers (Windows vs Unix)
# ------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  RM        := del /Q
  RMDIR     := rmdir /S /Q
  COPY      := copy /Y
  MKDIR_P   := if not exist "$(LIB_DIR)" mkdir "$(LIB_DIR)"
  NULLDEV   := NUL
  EXEEXT    := .exe
  TARGET_WIN        := $(TARGET)$(EXEEXT)
  TARGET_X86_64_WIN := $(TARGET_X86_64)$(EXEEXT)

  # Critical: $(MAKE) can be "C:/Program Files (x86)/.../make.exe"
  # cmd.exe splits on spaces unless quoted.
  MAKE_RECURSIVE := "$(MAKE)"
else
  RM        := rm -f
  RMDIR     := rm -rf
  COPY      := cp -f
  MKDIR_P   := mkdir -p $(LIB_DIR)
  NULLDEV   := /dev/null
  EXEEXT    :=
  TARGET_WIN        := $(TARGET)
  TARGET_X86_64_WIN := $(TARGET_X86_64)

  MAKE_RECURSIVE := $(MAKE)
endif

.PHONY: all clean distclean frontend

all: frontend $(TARGET_WIN) $(TARGET_X86_64_WIN) $(RUNTIME_OBJ)

# ------------------------------------------------------------
# Frontend (BNFC-generated) build
# ------------------------------------------------------------
frontend:
	$(MAKE_RECURSIVE) -C "$(FRONTEND_DIR)"

# ------------------------------------------------------------
# Runtime
# ------------------------------------------------------------
$(RUNTIME_OBJ): $(RUNTIME_SRC)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# C++ compilation
# ------------------------------------------------------------
$(SRC_DIR)/latte_main.o: $(SRC_DIR)/latte_main.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -c $< -o $@

$(SRC_DIR)/typecheck.o: $(SRC_DIR)/typecheck.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/env.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -c $< -o $@

$(SRC_DIR)/env.o: $(SRC_DIR)/env.cpp $(SRC_DIR)/env.hpp
	$(CXX) $(CXXFLAGS) -I"$(SRC_DIR)" -c $< -o $@

$(SRC_DIR)/latte_error.o: $(SRC_DIR)/latte_error.cpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(SRC_DIR)" -c $< -o $@

$(BACKEND_DIR)/codegen.o: $(BACKEND_DIR)/codegen.cpp $(BACKEND_DIR)/codegen.hpp \
  $(FRONTEND_DIR)/Absyn.H $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -c $< -o $@

$(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.hpp \
  $(BACKEND_DIR)/ir.hpp
	$(CXX) $(CXXFLAGS) -I"$(SRC_DIR)" -c $< -o $@

$(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.hpp \
  $(BACKEND_DIR)/ir.hpp $(BACKEND_DIR)/regalloc.hpp
	$(CXX) $(CXXFLAGS) -I"$(SRC_DIR)" -c $< -o $@

# ------------------------------------------------------------
# Link compiler (host)
# ------------------------------------------------------------
$(TARGET_WIN): $(CORE_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -o $@ \
	  $(CORE_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

$(TARGET_X86_64_WIN): $(TARGET_WIN)
ifeq ($(OS),Windows_NT)
	$(COPY) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)" >$(NULLDEV)
else
	$(COPY) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)"
endif

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------
clean:
clean:
ifeq ($(OS),Windows_NT)
	-$(RM) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)" 2>$(NULLDEV) || exit 0
	-$(RM) "$(SRC_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(BACKEND_DIR)\*.o" 2>$(NULLDEV) || exit 0

	-$(RM) "$(RUNTIME_OBJ)" 2>$(NULLDEV) || exit 0
	-$(RM) "$(LIB_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(LIB_DIR)/*.o" 2>$(NULLDEV) || exit 0

	-$(RM) "$(FRONTEND_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(FRONTEND_DIR)\TestLatteCPP.exe" "$(FRONTEND_DIR)\TestLatteCPP" 2>$(NULLDEV) || exit 0
	-$(RM) "$(FRONTEND_DIR)\LatteCPP.exe" "$(FRONTEND_DIR)\LatteCPP" 2>$(NULLDEV) || exit 0
	-$(RM) "$(FRONTEND_DIR)\LatteCPP.aux" "$(FRONTEND_DIR)\LatteCPP.log" "$(FRONTEND_DIR)\LatteCPP.pdf" "$(FRONTEND_DIR)\LatteCPP.dvi" "$(FRONTEND_DIR)\LatteCPP.ps" 2>$(NULLDEV) || exit 0
else
	$(RM) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)" "$(SRC_DIR)"/*.o "$(BACKEND_DIR)"/*.o "$(RUNTIME_OBJ)"
	$(MAKE_RECURSIVE) -C "$(FRONTEND_DIR)" clean || true
endif

distclean: clean
ifeq ($(OS),Windows_NT)
	-$(RMDIR) "$(FRONTEND_DIR)" 2>$(NULLDEV) || exit 0
else
	$(RMDIR) "$(FRONTEND_DIR)"
endif
