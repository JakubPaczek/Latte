CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
SEM_DIR      := $(SRC_DIR)/semantic
BACKEND_DIR  := $(SRC_DIR)/backend
LIB_DIR      := lib

TARGET        := latc
# TARGET_X86_64 := latc_x86_64   # (tymczasowo wyłączone)

# RUNTIME_SRC := $(LIB_DIR)/runtime.c
# RUNTIME_OBJ := $(LIB_DIR)/runtime.o

FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

# Two separate mains:
FRONTEND_MAIN_OBJ := $(SRC_DIR)/latc.o
# FULL_MAIN_OBJ     := $(SRC_DIR)/latc_x86_64.o

CORE_COMMON_OBJS := \
  $(SEM_DIR)/typecheck.o \
  $(SEM_DIR)/env.o \
  $(SEM_DIR)/latte_error.o

# BACKEND_OBJS := \
#   $(BACKEND_DIR)/codegen.o   \
#   $(BACKEND_DIR)/regalloc.o  \
#   $(BACKEND_DIR)/x86_emit.o

.PHONY: all clean distclean frontend

all: frontend $(TARGET)
# all: frontend $(TARGET) $(TARGET_X86_64) $(RUNTIME_OBJ)  # (wyłączone)

# ------------------------------------------------------------
# Frontend (BNFC-generated) build
# ------------------------------------------------------------
frontend:
	$(MAKE) -C $(FRONTEND_DIR)

# ------------------------------------------------------------
# Runtime (wyłączone)
# ------------------------------------------------------------
# $(RUNTIME_OBJ): $(RUNTIME_SRC)
# 	mkdir -p $(LIB_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# C++ compilation
# ------------------------------------------------------------

# frontend-only main
$(SRC_DIR)/main_frontend.o: $(SRC_DIR)/main_frontend.cpp \
  $(SEM_DIR)/typecheck.hpp $(SEM_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

# full compiler main (wyłączone)
# $(SRC_DIR)/latte_main.o: $(SRC_DIR)/latte_main.cpp \
#   $(SEM_DIR)/typecheck.hpp $(SEM_DIR)/latte_error.hpp
# 	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/typecheck.o: $(SEM_DIR)/typecheck.cpp \
  $(SEM_DIR)/typecheck.hpp $(SEM_DIR)/env.hpp $(SEM_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/env.o: $(SEM_DIR)/env.cpp $(SEM_DIR)/env.hpp
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

$(SEM_DIR)/latte_error.o: $(SEM_DIR)/latte_error.cpp $(SEM_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(SEM_DIR) -c $< -o $@

# backend compilation (wyłączone)
# $(BACKEND_DIR)/codegen.o: $(BACKEND_DIR)/codegen.cpp $(BACKEND_DIR)/codegen.hpp \
#   $(FRONTEND_DIR)/Absyn.H $(BACKEND_DIR)/ir.hpp
# 	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -c $< -o $@
#
# $(BACKEND_DIR)/regalloc.o: $(BACKEND_DIR)/regalloc.cpp $(BACKEND_DIR)/regalloc.hpp \
# D $(BACKEND_DIR)/ir.hpp
# 	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@
#
# $(BACKEND_DIR)/x86_emit.o: $(BACKEND_DIR)/x86_emit.cpp $(BACKEND_DIR)/x86_emit.hpp \
#   $(BACKEND_DIR)/ir.hpp $(BACKEND_DIR)/regalloc.hpp
# 	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# ------------------------------------------------------------
# Link compiler (host)
# ------------------------------------------------------------

# latc = frontend-only
$(TARGET): frontend $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -o $@ \
	  $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)
	  
# latc_x86_64 = FULL COMPILER (wyłączone)
# $(TARGET_X86_64): frontend $(FULL_MAIN_OBJ) $(CORE_COMMON_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS) $(RUNTIME_OBJ)
# 	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -I$(SRC_DIR) -I$(SEM_DIR) -o $@ \
# 	  $(FULL_MAIN_OBJ) $(CORE_COMMON_OBJS) $(BACKEND_OBJS) $(FRONTEND_OBJS)

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------
clean:
	rm -f $(TARGET)
	# rm -f $(TARGET_X86_64)  # (wyłączone)

	rm -f $(SRC_DIR)/*.o $(SEM_DIR)/*.o $(BACKEND_DIR)/*.o
	# rm -f $(RUNTIME_OBJ) $(LIB_DIR)/*.o  # (wyłączone)

	$(MAKE) -C $(FRONTEND_DIR) clean || true

	# tests artifacts (opcjonalnie; możesz odkomentować jak chcesz)
	# rm -f lattests/good/*.s lattests/good/*.o
	# rm -f lattests/good/*.got lattests/good/*.diff lattests/good/*.log lattests/good/*.err
	# find lattests/good -maxdepth 1 -type f -name 'core*' -executable -delete 2>/dev/null || true

distclean: clean
	# jeśli frontend jest generowany (BNFC), to zwykle nie chcesz go usuwać tutaj
	# rm -rf $(FRONTEND_DIR)
