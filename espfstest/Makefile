CFLAGS=-I../lib/heatshrink -I../user -I../include -std=gnu99 

espfstest: main.o espfs.o heatshrink_decoder.o
	$(CC) -o $@ $^

espfs.o: espfs.c ../user/espfs.c


clean:
	rm -f *.o espfstest
