CFLAGS = -Wall -lpthread -L/usr/lib64/mysql -lmysqlclient
CC = g++

ifdef DEBUG
CFLAGS += -g
LDFLAGS += -g
endif

TARGETS = pooltest

all: $(TARGETS)

pooltest: pool_test.o mysql_connection_pool.o
	$(CC) $(CFLAGS) -o $@ $^

%.o:%.cpp
	$(CC) $(CFLAGS) -c -o $@ $^ $(INC_PATH) -pg
%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(INC_PATH) -pg

clean:
	rm -f $(TARGETS) *.o
