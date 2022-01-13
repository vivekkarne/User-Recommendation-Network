CC = g++

all: serverA serverB servermain client

serverA: serverA.cpp
	$(CC) -o serverA serverA.cpp

serverB: serverB.cpp
	$(CC) -o serverB serverB.cpp

servermain: servermain.cpp
	$(CC) -o servermain servermain.cpp

client: client.cpp
	$(CC) -o client client.cpp

run_serverA: serverA
	./serverA

run_serverB: serverB
	./serverB

run_servermain: servermain
	./servermain

run_client: client
	./client

clean: 
	$(RM) serverA serverB servermain client
