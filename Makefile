ANSWER=0
CFLAGS=-g -std=gnu99

all: rkmatch bloom_test

rkmatch: rkmatch.o bloom.o
	gcc -lm $< bloom.o -o $@ -lrt

bloom_test : bloom_test.o bloom.o
	gcc -lm $< bloom.o -o $@

%.o : %.c
	gcc $(CFLAGS) -DANSWER=$(ANSWER) -c ${<}

handin:
	tar -cvf lab2.tar.gz *.c *.h Makefile

clean :
	rm -f *.o rkmatch bloom_test *.dvi *.log *.tar 
