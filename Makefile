all: mush2
 
mush2: mush2.c
	gcc -c -g -Wall -I  ~pn-cs357/Given/Mush/include -o mush2.o mush2.c
	gcc -g -Wall -L  ~pn-cs357/Given/Mush/lib64 -o mush2 mush2.o -lmush
 
test: all
	
 
clean:
	rm -f all a.out
