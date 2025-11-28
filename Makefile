# Kompilator i flagi
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

# Ścieżki
SRC_DIR      := src
FRONTEND_DIR := $(SRC_DIR)/frontend

# BNFC
BNFC       := bnfc
BNFC_FLAGS := --cpp -m
BNFC_INPUT := $(SRC_DIR)/LatteCPP.cf

# Docelowy program
TARGET := latc

# Obiekty z BNFC (gotowe po "make -C src/frontend")
FRONTEND_OBJS := \
  $(FRONTEND_DIR)/Absyn.o   \
  $(FRONTEND_DIR)/Buffer.o  \
  $(FRONTEND_DIR)/Parser.o  \
  $(FRONTEND_DIR)/Printer.o \
  $(FRONTEND_DIR)/Lexer.o

# Twoje obiekty
BACKEND_OBJS := \
  $(SRC_DIR)/latte_main.o    \
  $(SRC_DIR)/typecheck.o     \
  $(SRC_DIR)/env.o           \
  $(SRC_DIR)/latte_error.o

.PHONY: all clean distclean frontend

all: $(TARGET)

# ============================================================
# Kompilacja plików C++
# ============================================================

$(SRC_DIR)/latte_main.o: $(SRC_DIR)/latte_main.cpp $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -c $< -o $@

$(SRC_DIR)/typecheck.o: $(SRC_DIR)/typecheck.cpp $(SRC_DIR)/typecheck.hpp $(SRC_DIR)/env.hpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -c $< -o $@

$(SRC_DIR)/env.o: $(SRC_DIR)/env.cpp $(SRC_DIR)/env.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/latte_error.o: $(SRC_DIR)/latte_error.cpp $(SRC_DIR)/latte_error.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# Linkowanie
# ============================================================

$(TARGET): frontend $(BACKEND_OBJS)
	$(CXX) $(CXXFLAGS) -I$(FRONTEND_DIR) -o $@ $(BACKEND_OBJS) $(FRONTEND_OBJS)

# ============================================================
# Sprzątanie
# ============================================================

clean:
	rm -f $(TARGET) $(SRC_DIR)/*.o

distclean: clean
	rm -rf $(FRONTEND_DIR)