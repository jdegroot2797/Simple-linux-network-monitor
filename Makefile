CC=g++
CFLAGS=-I
CFLAGS+=-Wall
FILES1=networkMonitor.cpp
FILES2=intfMonitor.cpp

networkMonitor: $(FILES1)
	$(CC) $(CFLAGS) -o networkMonitor $(FILES1)

intfMonitor: $(FILES2)
	$(CC) $(CFLAGS) -o intfMonitor $(FILES2)

clean:
	rm -f *.o networkMonitor intfMonitor

all: networkMonitor intfMonitor
