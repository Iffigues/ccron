CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
TARGET = ccron

OBJS = main.o cron.o validate.o json.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c cron.h
	$(CC) $(CFLAGS) -c main.c -o main.o

cron.o: cron.c cron.h
	$(CC) $(CFLAGS) -c cron.c -o cron.o

validate.o: validate.c cron.h
	$(CC) $(CFLAGS) -c validate.c -o validate.o

json.o: json.c cron.h
	$(CC) $(CFLAGS) -c json.c -o json.o

clean:
	rm -f *.o $(TARGET)