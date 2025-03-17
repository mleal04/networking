CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g
CLIENT = client
BASE64_SRC = base64-decode.c
BASE64_OBJ = base64-decode.o
CLIENT_SRC = client.c
CLIENT_OBJ = client.o

# All targets
all: $(CLIENT)

# Rule to compile the client executable
$(CLIENT): $(CLIENT_OBJ) $(BASE64_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_OBJ) $(BASE64_OBJ)

# Rule to compile client object
$(CLIENT_OBJ): $(CLIENT_SRC) base64-decode.h
	$(CC) $(CFLAGS) -c $(CLIENT_SRC)

# Rule to compile base64 decode object
$(BASE64_OBJ): $(BASE64_SRC) base64-decode.h
	$(CC) $(CFLAGS) -c $(BASE64_SRC)

# Clean up compiled files
clean:
	rm -f $(CLIENT) $(CLIENT_OBJ) $(BASE64_OBJ)