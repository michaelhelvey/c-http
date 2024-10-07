OBJECTS = kqueue conn handler tcp

SRC_DIR = src
BUILD_DIR = build
CC = clang
CFLAGS = -Wall -Werror -Wextra -std=c17 -g
LDFLAGS = -lpthread

MAIN = main
TEST_MAIN = test_main

# Output targets
TARGET = $(BUILD_DIR)/http
TARGET_TEST = $(BUILD_DIR)/http_test

# Object files with paths (build/<file>.o)
OBJS = $(patsubst %, $(BUILD_DIR)/%.o, $(OBJECTS))

# Default target: build both programs
all: $(TARGET) $(TARGET_TEST)

# Rule to build the main program
$(TARGET): $(OBJS) $(BUILD_DIR)/$(MAIN).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Rule to build the test program
$(TARGET_TEST): $(OBJS) $(BUILD_DIR)/$(TEST_MAIN).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Compile source files into object files (src/<file>.c -> build/<file>.o)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure the build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean the build directory
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
