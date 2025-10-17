CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = inicializador emisor receptor finalizador

all: $(TARGETS)

initializer: inicializador.c memoria_compartida.h
	$(CC) $(CFLAGS) -o $@ $<

emitter: emisor.c memoria_compartida.h
	$(CC) $(CFLAGS) -o $@ $<

receiver: receptor.c memoria_compartida.h
	$(CC) $(CFLAGS) -o $@ $<

finalizer: finalizador.c memoria_compartida.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) output.txt

.PHONY: all clean
