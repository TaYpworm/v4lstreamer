CC=g++
CFLAGS= -g

all: v4lstreamer.o IOException.o
	$(CC) $(CFLAGS) -o example v4lstreamer.o IOException.o example.cpp

v4lstreamer.o: v4lstreamer.cpp v4lstreamer.h
	$(CC) -c v4lstreamer.cpp

IOException.o: IOException.cpp IOException.h
	$(CC) -c IOException.cpp

clean:
	rm *.o example
