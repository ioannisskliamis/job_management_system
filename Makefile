# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

# Directories
SRC_DIR = src
BUILD_DIR = build

# Executables
COORD_TARGET = jms_coord
CONSOLE_TARGET = jms_console

# Shared logic (files used by both)
SHARED_SRCS = src/jobs_list.c
SHARED_OBJS = $(BUILD_DIR)/jobs_list.o

# Coordinator specific
COORD_SRCS = src/jms_coord.c \
			 src/coordinator.c \
			 src/pool.c
COORD_OBJS = $(BUILD_DIR)/jms_coord.o $(BUILD_DIR)/coordinator.o $(BUILD_DIR)/pool.o

# Console specific
CONSOLE_SRCS = src/jms_console.c \
			   src/console.c
CONSOLE_OBJS = $(BUILD_DIR)/jms_console.o $(BUILD_DIR)/console.o

# Default target: builds both
all: $(BUILD_DIR) $(COORD_TARGET) $(CONSOLE_TARGET)

# Build Coordinator: links shared objects + coord objects
$(COORD_TARGET): $(COORD_OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Build Console: links shared objects + console objects
$(CONSOLE_TARGET): $(CONSOLE_OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Pattern rule for compiling .c to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(COORD_TARGET) $(CONSOLE_TARGET)

.PHONY: all clean