newshell: main.o
		gcc -o output main.o -lm

main.o: main.c
		gcc -c main.c -lm

clean:
		rm -f *.o output
