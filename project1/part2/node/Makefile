CC = gcc
CFLAGS = -Wall -Wextra
TARGET = node
OBJ = node.o base64-encode.o index-test.o

# Default target
all: $(TARGET)

# Link the object files to create the final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Compile node.c (main server) into node.o
node.o: node.c base64-encode.h index-test.h
	$(CC) $(CFLAGS) -c node.c

# Compile base64-encode.c into base64-encode.o
base64-encode.o: base64-encode.c base64-encode.h
	$(CC) $(CFLAGS) -c base64-encode.c

# Compile index-test.c into index-test.o
index-test.o: index-test.c index-test.h
	$(CC) $(CFLAGS) -c index-test.c

# Clean up object files and executable
clean:
	rm -f $(OBJ) $(TARGET)
