build:
	gcc -fPIC -c so_stdio.c
	gcc -shared so_stdio.o -o libso_stdio.so
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.

clean:
	rm -rf *.o libso_stdio.so