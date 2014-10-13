CFLAGS=-I../lib/heatshrink -I../user -I../include -std=gnu99 

espfs.o: espfs.c ../user/espfs.c

espfstest: main.o espfs.o heatshrink_decoder.o
	$(CC) -o $@ $^


clean:
	rm -f *.o espfstest
