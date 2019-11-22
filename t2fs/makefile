#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: t2fs
	ar csr $(LIB_DIR)/libt2fs.a $(BIN_DIR)/*.o $(LIB_DIR)/*.o

t2fs: $(SRC_DIR)/t2fs.c $(INC_DIR)/*.h
	$(CC) -o $(BIN_DIR)/t2fs.o -c $(SRC_DIR)/t2fs.c -Wall

teste1: all
	$(CC) -o teste1 teste1.c -L$(LIB_DIR) -lt2fs -Wall -lm

teste_link: all
	$(CC) -o teste_link teste_link.c -L$(LIB_DIR) -lt2fs -Wall -lm

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/t2fs.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
	rm -f teste1
	rm -f teste_link


