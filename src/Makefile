CFLAGS=-Wall -g -Iinclude

all: ring

main.o ring_cln.o ring_srv.o: include/ring.h
ring_cln.o ring_srv.o common.o: include/common.h

ring: main.o ring_cln.o ring_srv.o common.o
	$(CC) -o $@ $^ -lpthread

clean:
	rm -f *.o ring
