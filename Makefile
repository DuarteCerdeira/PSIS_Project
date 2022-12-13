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
	$(CC) ./bin/$< $(HEADERS) -o chase-client$(EXT) $(LFLAGS)

# Server executable
server: chase-server.o $(HEADERS)
	$(CC) ./bin/$< $(HEADERS) -o chase-server$(EXT) $(LFLAGS)

# Client object files
chase-client.o:
	$(CC) $(CFLAGS) -c $(CLIENT_PATH) -o ./bin/chase-client.o

# Server object files
chase-server.o:
	$(CC) $(CFLAGS) -c $(SERVER_PATH) -o ./bin/chase-server.o

# Zip
zip: ./src/$* ./Makefile ./bin
	zip -r gr3_96133_96195 $^

# Clean
clean:
	rm ./bin/*.o
	rm *.out
