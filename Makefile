# Compiler
CC := gcc
# Compiler flags
CFLAGS := -Wall -g
# Linker flags
LFLAGS := -lncurses
# Header files
HEADERS := $(wildcard $(SRCPATH)/*.h) ./lib/*.h
# Client source code path
CLIENT_PATH := ./src/clients/chase-client.c
# Server source code path
SERVER_PATH := ./src/server/chase-server.c
# Board source code path
BOARD_PATH := ./src/board.c
# Executable extension
BOTS_PATH := ./src/clients/chase-bots.c
EXT := .out

# NOTES: 
# - $< is the first dependency
# - $@ is the target
# - $^ is all dependencies

# Default target
all: client server bots

# Client executable
client: chase-client.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-client$(EXT) $(LFLAGS)

# Server executable
server: chase-server.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-server$(EXT) $(LFLAGS)

# Bots executables
bots: chase-bots.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-bots$(EXT) $(LFLAGS)

# Board object files
board.o: $(BOARD_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(BOARD_PATH) -o ./obj/board.o

# Client object files
chase-client.o: $(CLIENT_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(CLIENT_PATH) -o ./obj/chase-client.o

# Server object files
chase-server.o: $(SERVER_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SERVER_PATH) -o ./obj/chase-server.o

# Bots object files
chase-bots.o: $(BOTS_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(BOTS_PATH) -o ./obj/chase-bots.o

# Zip
zip: ./src/$* ./Makefile ./bin
	zip -r gr3_96133_96195 $^

# Clean
clean:
	rm ./bin/*.out
	rm ./obj/*.o
