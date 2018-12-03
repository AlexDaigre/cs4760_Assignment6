CC      = gcc
CFLAGS  = -g
TARGET1 = oss
OBJS1   = main.o
TARGET2 = worker
OBJS2   = worker.o

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1) -pthread

$(TARGET2): $(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2) -pthread

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c


clean:
	/bin/rm -f *.o $(TARGET)