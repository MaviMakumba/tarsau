CC = gcc
CFLAGS = -Wall -I./include

SRC = ./src/main.c
OUT = ./bin/tarsau

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run:
	./bin/tarsau

clean:
	rm -f ./bin/tarsau