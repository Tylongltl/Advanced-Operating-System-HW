all:clean server client

server:hw2_server.o
	gcc -o server hw2_server.o
client:hw2_client.o
	gcc -o client hw2_client.o
server.o:hw2_server.c
	gcc -c hw2_server.c
client.o:hw2_client.c
	gcc -c hw2_client.c
clean:
	rm -f hw2_client hw2_server client server *.o
