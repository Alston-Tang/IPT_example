CC=g++
CFLAGS=

default:
	$(CC) -o pt pt.cpp $(CFLAGS)
clean:
	rm ./pt
