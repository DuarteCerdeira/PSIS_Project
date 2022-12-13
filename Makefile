CC := gcc									# Compiler
CFLAGS := -Wall -g							# Compiler flags
LFLAGS := -lncurses							# Linker flags
HEADERS := $(wildcard $(SRCPATH)/*.h)		# Header files
CLIENT_PATH := ./src/clients/chase-client.c	# Client source code path
SERVER_PATH := ./src/server/chase-server.c	# Server source code path
EXT := .out									# Executable extension

# NOTES: 
# - $< is the first dependency
# - $@ is the target
# - $^ is all dependencies

# Default target
all: client server

# Client executable
client: chase-client.o $(HEADERS)
	$(CC) ./obj/$< $(HEADERS) -o ./bin/chase-client$(EXT) $(LFLAGS)

# Server executable
server: chase-server.o $(HEADERS)
	$(CC) ./obj/$< $(HEADERS) -o ./bin/chase-server$(EXT) $(LFLAGS)

# Client object files
chase-client.o:
	$(CC) $(CFLAGS) -c $(CLIENT_PATH) -o ./obj/chase-client.o

# Server object files
chase-server.o:
	$(CC) $(CFLAGS) -c $(SERVER_PATH) -o ./obj/chase-server.o

# Zip
zip: ./src/$* ./Makefile ./bin
	zip -r gr3_96133_96195 $^

# Clean
clean:
	rm ./bin/*.out
	rm ./obj/*.o
