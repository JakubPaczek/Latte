CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
BACKEND_DIR  := $(SRC_DIR)/backend
SEM_DIR 	 := $(SRC_DIR)/semantic
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

# Two separate mains:
FRONTEND_MAIN_OBJ := $(SRC_DIR)/main_frontend.o
FULL_MAIN_OBJ     := $(SRC_DIR)/latte_main.o   # <-- your existing full compiler main

CORE_COMMON_OBJS := \
  $(SEM_DIR)/typecheck.o \
  $(SEM_DIR)/env.o \
  $(SEM_DIR)/latte_error.o

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

# NEW: frontend-only main
$(SRC_DIR)/main_frontend.o: $(SRC_DIR)/main_frontend.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -c $< -o $@

# Existing full compiler main (unchanged file name)
$(SRC_DIR)/latte_main.o: $(SRC_DIR)/latte_main.cpp \
  $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -c $< -o $@

$(SEM_DIR)/typecheck.o: $(SEM_DIR)/typecheck.cpp \
  $(SEM_DIR)/typecheck.hpp $(SEM_DIR)/env.hpp $(SEM_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -I"$(SEM_DIR)" -c $< -o $@

$(SEM_DIR)/env.o: $(SEM_DIR)/env.cpp $(SEM_DIR)/env.hpp
	$(CXX) $(CXXFLAGS) -I"$(SEM_DIR)" -c $< -o $@

$(SEM_DIR)/latte_error.o: $(SEM_DIR)/latte_error.cpp $(SEM_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I"$(SEM_DIR)" -c $< -o $@

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

# latc = FRONTEND ONLY
$(TARGET_WIN): frontend $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -o $@ \
	  $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)

# latc_x86_64 = FULL COMPILER (frontend + backend)
$(TARGET_X86_64_WIN): frontend $(FULL_MAIN_OBJ) $(CORE_COMMON_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS) $(RUNTIME_OBJ)
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -o $@ \
	  $(FULL_MAIN_OBJ) $(CORE_COMMON_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)" 2>$(NULLDEV) || exit 0

	# object files (your code)
	-$(RM) "$(SRC_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(BACKEND_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(SEM_DIR)\*.o" 2>$(NULLDEV) || exit 0

	# runtime
	-$(RM) "$(RUNTIME_OBJ)" 2>$(NULLDEV) || exit 0
	-$(RM) "$(LIB_DIR)\*.o" 2>$(NULLDEV) || exit 0
	-$(RM) "$(LIB_DIR)/*.o" 2>$(NULLDEV) || exit 0

	# BNFC frontend artifacts (either do it manually OR call sub-make; better: call sub-make)
	$(MAKE_RECURSIVE) -C "$(FRONTEND_DIR)" clean || exit 0

	# tests artifacts (Windows)
	-$(RM) "lattests\good\*.s" "lattests\good\*.o" "lattests\good\*.exe" 2>$(NULLDEV) || exit 0
	-$(RM) "lattests\good\*.got" "lattests\good\*.diff" 2>$(NULLDEV) || exit 0
	-$(RM) "lattests\good\*.log" "lattests\good\*.err" 2>$(NULLDEV) || exit 0

else
	# binaries + object files
	$(RM) "$(TARGET_WIN)" "$(TARGET_X86_64_WIN)" \
	  "$(SRC_DIR)"/*.o "$(BACKEND_DIR)"/*.o "$(SEM_DIR)"/*.o "$(RUNTIME_OBJ)"

	# BNFC frontend artifacts
	$(MAKE_RECURSIVE) -C "$(FRONTEND_DIR)" clean || true

	# tests artifacts (Unix/WSL/Linux)
	$(RM) lattests/good/*.s lattests/good/*.o
	$(RM) lattests/good/*.got lattests/good/*.diff lattests/good/*.log lattests/good/*.err
	-find lattests/good -maxdepth 1 -type f -name 'core*' -executable -delete 2>/dev/null || true
endif

distclean: clean
ifeq ($(OS),Windows_NT)
	-$(RMDIR) "$(FRONTEND_DIR)" 2>$(NULLDEV) || exit 0
else
	$(RMDIR) "$(FRONTEND_DIR)"
endif
