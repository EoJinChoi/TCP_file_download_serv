CC = gcc
CFLAGS = -Wall -Werror

SERVER_SRC = task2_receiver.c

CLIENT_TARGET = client
SERVER_TARGET = server

$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_SRC)

clean:
	rm -f $(SERVER_TARGET)