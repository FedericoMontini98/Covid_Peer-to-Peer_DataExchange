# che dipende dai target client e server scritti sotto
all: peer ds
# make rule per il client
peer: peer.o
	gcc -Wall peer.o -o peer
# make rule per il server
ds: ds.o
	gcc -Wall discoveryServer.o -o ds

peer.o: utility.h peer.c
	gcc -c -Wall peer.c
ds.o: discoveryServer.c
	gcc -c -Wall discoveryServer.c
	 
# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o peer discoveryServer