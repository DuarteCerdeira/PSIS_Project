# Compiler
CC := gcc
# Compiler flags
CFLAGS := -Wall -g
# Linker flags
LFLAGS := -lncurses -lpthread
# Header files
HEADERS := $(wildcard $(SRCPATH)/*.h) ./lib/*.h
# Client source code path
CLIENT_PATH := ./src/clients/chase-client.c
# Server source code path
SERVER_PATH := ./src/server/chase-server.c
# Board source code path
BOARD_PATH := ./lib/board.c
# Stack source code path
STACK_PATH := ./lib/stack.c

# Executable extension
EXT := .out

# NOTES: 
# - $< is the first dependency
# - $@ is the target
# - $^ is all dependencies

# Default target
all: client server

# Client executable
client: chase-client.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-client$(EXT) $(LFLAGS)

# Server executable
server: chase-server.o board.o stack.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-server$(EXT) $(LFLAGS)


# Board object files
board.o: $(BOARD_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(BOARD_PATH) -o ./obj/board.o

# Stack object files
stack.o: $(STACK_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(STACK_PATH) -o ./obj/stack.o

# Client object files
chase-client.o: $(CLIENT_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(CLIENT_PATH) -o ./obj/chase-client.o

# Server object files
chase-server.o: $(SERVER_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SERVER_PATH) -o ./obj/chase-server.o


# Zip
zip: ./src/$* ./Makefile ./bin
	zip -r gr3_96133_96195 $^

# Clean
clean:
	rm ./bin/*.out
	rm ./obj/*.o
