# Compiler and flags
CXX       := clang++
CXXFLAGS  := -std=c++23 -O3 -march=native -Wall -Wextra -Wpedantic

# Target
TARGET    := collatz
SRC       := collatz.cpp
OBJ       := $(SRC:.cpp=.o)

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

# Compile
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run target
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean run
