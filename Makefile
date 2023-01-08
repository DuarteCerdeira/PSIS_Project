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

PRIZES_PATH := ./src/clients/chase-prizes.c
# Executable extension
EXT := .out

# NOTES: 
# - $< is the first dependency
# - $@ is the target
# - $^ is all dependencies

# Default target
all: client server prizes

# Client executable
client: chase-client.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-client$(EXT) $(LFLAGS)

# Server executable
server: chase-server.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-server$(EXT) $(LFLAGS)


# Prizes executables
prizes: chase-prizes.o board.o
	$(CC) $(addprefix ./obj/, $^) -o ./bin/chase-prizes$(EXT) $(LFLAGS)

# Board object files
board.o: $(BOARD_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(BOARD_PATH) -o ./obj/board.o

# Client object files
chase-client.o: $(CLIENT_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(CLIENT_PATH) -o ./obj/chase-client.o

# Server object files
chase-server.o: $(SERVER_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SERVER_PATH) -o ./obj/chase-server.o


# Prizes object files
chase-prizes.o: $(PRIZES_PATH) $(HEADERS)
	$(CC) $(CFLAGS) -c $(PRIZES_PATH) -o ./obj/chase-prizes.o

# Zip
zip: ./src/$* ./Makefile ./bin
	zip -r gr3_96133_96195 $^

# Clean
clean:
	rm ./bin/*.out
	rm ./obj/*.o
