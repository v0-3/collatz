# Compiler and flags
ifeq ($(origin CXX), default)
CXX       := clang++
endif
CXX       ?= clang++
CXXFLAGS  ?= -std=c++23 -O3 -march=native -Wall -Wextra -Wpedantic
THREAD_FLAGS := -pthread

# Target
TARGET    := collatz
SRC       := collatz.cpp
OBJ       := $(SRC:.cpp=.o)
TEST_TARGET := collatz_test
TEST_OBJ  := $(TEST_TARGET).o

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(LDFLAGS) $(OBJ) $(LDLIBS) -o $(TARGET)

$(TEST_TARGET): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(LDFLAGS) $(TEST_OBJ) $(LDLIBS) -o $(TEST_TARGET)

# Compile
%.o: %.cpp collatz.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) -c $< -o $@

test: $(TEST_TARGET) $(TARGET)
	./$(TEST_TARGET)
	@output=$$(./$(TARGET) unexpected 2>&1); status=$$?; \
	if [ $$status -ne 1 ] || [ "$$output" != "Usage: ./$(TARGET)" ]; then \
		echo "FAIL: unexpected command-line argument handling"; exit 1; \
	fi

# Run target
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJ) $(TEST_TARGET) $(TEST_OBJ)

.PHONY: all clean run test
