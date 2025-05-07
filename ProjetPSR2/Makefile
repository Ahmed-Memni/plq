# --- Makefile ---
CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lpthread
SRC_IO = src/file_io.c
SRC_SRV = src/server.c
SRC_CLI = src/client.c
SRC_HIST = src/histo_io.c
SRC_FACT = src/facture_io.c
OBJ_IO = $(SRC_IO:.c=.o)
OBJ_SRV = $(SRC_SRV:.c=.o)
OBJ_CLI = $(SRC_CLI:.c=.o)
OBJ_HIST = $(SRC_HIST:.c=.o)
OBJ_FACT = $(SRC_FACT:.c=.o)
BIN_DIR = bin
.PHONY: all clean
all: $(BIN_DIR)/server $(BIN_DIR)/client
$(BIN_DIR)/server: $(OBJ_IO) $(OBJ_HIST) $(OBJ_FACT) $(OBJ_SRV) src/vol.h src/histo.h src/facture.h
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ_IO) $(OBJ_HIST) $(OBJ_FACT) $(OBJ_SRV) $(LDFLAGS)
$(BIN_DIR)/client: $(OBJ_CLI) $(OBJ_HIST) $(OBJ_FACT) src/histo.h src/facture.h
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ_CLI) $(OBJ_HIST) $(OBJ_FACT) $(LDFLAGS)
# Generic rule for object files
src/%.o: src/%.c src/vol.h src/histo.h src/facture.h
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf $(BIN_DIR) src/*.o
