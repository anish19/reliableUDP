CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: server client 

server: dg_server.o get_ifi_info_plus.o readline.o
	${CC} ${FLAGS} -o server dg_server.o get_ifi_info_plus.o readline.o ${LIBS}
dg_server.o: dg_server.c
	${CC} ${CFLAGS} -c dg_server.c


client: dg_client.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o client dg_client.o get_ifi_info_plus.o ${LIBS}
dg_client.o: dg_client.c
	${CC} ${CFLAGS} -c dg_client.c

get_ifi_info_plus.o : /home/courses/cse533/Asgn2_code/get_ifi_info_plus.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Asgn2_code/get_ifi_info_plus.c

readline.o: /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c

clean:
	rm server dg_server.o client dg_client.o readline.o


