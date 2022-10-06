OBJECTS = lets-talk

all: $(OBJECTS)

lets-talk: lets-talk.c list.c list.h
	gcc -g -Wall -o lets-talk -ggdb3 lets-talk.c list.c -lpthread -lrt

.PHONY: clean valgrind

valgrind:
	valgrind -s ./lets-talk 3031 127.0.0.1 3030

clean:
	rm $(OBJECTS)