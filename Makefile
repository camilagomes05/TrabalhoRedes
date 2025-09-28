CC=gcc
CFLAGS=-Wall -pthread
EXECUTABLE=peer

all: $(EXECUTABLE)

$(EXECUTABLE): peer.c
	$(CC) $(CFLAGS) peer.c -o $(EXECUTABLE)

clean:
	rm -f $(EXECUTABLE)