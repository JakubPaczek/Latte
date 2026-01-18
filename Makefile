CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
CFLAGS   := -Wall -Wextra -O2 -g

SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend
SEM_DIR      := $(SRC_DIR)/semantic
BACKEND_DIR  := $(SRC_DIR)/backend
LIB_DIR      := lib

TARGET := latc

# ---- BNFC ----
# Podmień, jeśli Twoja gramatyka ma inną nazwę lub leży gdzie indziej.
GRAMMAR := LatteCPP.cf

# Plik “pieczątka” – służy do tego, żeby BNFC nie odpalać bez potrzeby.
BNFC_STAMP := $(FRONTEND_DIR)/.bnfc.stamp

FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

FRONTEND_MAIN_OBJ := $(SRC_DIR)/latc.o

CORE_COMMON_OBJS := \
  $(SEM_DIR)/typecheck.o \
  $(SEM_DIR)/env.o \
  $(SEM_DIR)/latte_error.o

.PHONY: all frontend bnfc clean distclean

all: $(TARGET)

# ------------------------------------------------------------
# BNFC generation (runs only when grammar changed)
# ------------------------------------------------------------
bnfc: $(BNFC_STAMP)

$(BNFC_STAMP): $(GRAMMAR)
	@echo "[BNFC] Generating C++ frontend into $(FRONTEND_DIR) from $(GRAMMAR)"
	@mkdir -p $(FRONTEND_DIR)
	bnfc --cpp -o $(FRONTEND_DIR) $(GRAMMAR)
	@touch $(BNFC_STAMP)

# ------------------------------------------------------------
# Frontend build (BNFC must exist before calling sub-make)
# ------------------------------------------------------------
frontend: bnfc
	$(MAKE) -C $(FRONTEND_DIR)

# ------------------------------------------------------------
# C++ compilation
# ------------------------------------------------------------
$(SRC_DIR)/latc.o: $(SRC_DIR)/latc.cpp
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -I"$(SEM_DIR)" -c $< -o $@

$(SEM_DIR)/typecheck.o: $(SEM_DIR)/typecheck.cpp \
  $(SEM_DIR)/typecheck.h $(SEM_DIR)/env.h $(SEM_DIR)/latte_error.h
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -I"$(SEM_DIR)" -c $< -o $@

$(SEM_DIR)/env.o: $(SEM_DIR)/env.cpp $(SEM_DIR)/env.h
	$(CXX) $(CXXFLAGS) -I"$(SEM_DIR)" -c $< -o $@

$(SEM_DIR)/latte_error.o: $(SEM_DIR)/latte_error.cpp $(SEM_DIR)/latte_error.h
	$(CXX) $(CXXFLAGS) -I"$(SEM_DIR)" -c $< -o $@

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------
$(TARGET): frontend $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) -I"$(FRONTEND_DIR)" -I"$(SRC_DIR)" -o $@ \
	  $(FRONTEND_MAIN_OBJ) $(CORE_COMMON_OBJS) $(FRONTEND_OBJS)

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------
clean:
	rm -f $(TARGET)
	rm -f $(SRC_DIR)/*.o $(SEM_DIR)/*.o $(BACKEND_DIR)/*.o
	$(MAKE) -C $(FRONTEND_DIR) clean || true

# distclean usuwa też wszystkie pliki wygenerowane przez BNFC
distclean: clean
	rm -f $(BNFC_STAMP)
	rm -f $(FRONTEND_DIR)/Absyn.* \
	      $(FRONTEND_DIR)/Parser.* \
	      $(FRONTEND_DIR)/Lexer.* \
	      $(FRONTEND_DIR)/Printer.* \
	      $(FRONTEND_DIR)/Buffer.* \
	      $(FRONTEND_DIR)/Skeleton.* \
	      $(FRONTEND_DIR)/Test.* \
	      $(FRONTEND_DIR)/Layout.* \
	      $(FRONTEND_DIR)/Makefile
