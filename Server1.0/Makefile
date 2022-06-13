CC=g++
CPPFLAGS= 
CFLAGS=
LDFLAGS=

clientSrc=client.cpp
clientObj=$(patsubst %.c,%.o,$(clientSrc))
serverSrc= main.cpp ./src/utils.cpp ./src/Timer.cpp ./src/http_conn.cpp ./src/Epoller.cpp ./src/conn_pool.cpp ./src/multipServer.cpp
serverObj=$(patsubst %.c,%.o,$(serverSrc))

client: $(clientObj)
	g++ -o $@ $^
server: $(serverObj)
	g++ -o $@ $^

%.o:%.c
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

clean:
	rm -f server client