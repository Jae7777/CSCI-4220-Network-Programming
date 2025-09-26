# The compiler to use
CC = gcc
# Compiler flags
CFLAGS = -I../unpv13e-master/unpv13e-master/lib -g -O2 -Wall
# Linker flags
LDFLAGS = -L../unpv13e-master/unpv13e-master -lunp -pthread

# The target executable
TARGET = tftp.out

# The source files
SRCS = tftp_server.c

# The object files
OBJS = $(SRCS:.c=.o)

# Default rule
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile a .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up rule
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
