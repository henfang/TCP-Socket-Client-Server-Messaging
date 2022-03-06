# Henry Fang, Aasrija Puchakatla
# hef052, aap435
# CMPT332
# A4

all: clientR clientS server

gdb: server.c clientR.c clientS.c
	gcc -g -Wall -o server server.c -pthread
	gcc -g -Wall -o clientR clientR.c
	gcc -g -Wall -o clientS clientS.c

clientR: clientR.c 
	gcc -Wall -o clientR clientR.c
clientS: clientS.c 
	gcc -Wall -o clientS clientS.c
server: server.c
	gcc -Wall -o server server.c -pthread

tar:
	tar -cvf CMPT332_A4.tar server.c clientR.c clientS.c Makefile cmpt332A4Documentation.pdf

clean:
	rm -f *.o
	rm -f clientR
	rm -f clientS
	rm -f server
