all:
	gcc -o server server.c
	gcc -o client client.c -lpthread
	./server 9001
