CC = gcc -g
OBJECTS = pairctl.o
INCS  = -I /usr/local/BerkeleyDB.5.1/include
DEPS = -L /usr/local/BerkeleyDB.5.1/lib

pairctl : $(OBJECTS)
	$(CC) -o pairctl -ldb $(OBJECTS) $(DEPS)

pairctl.o: pairctl.c
	$(CC) -c pairctl.c $(INCS)

.PHONY : clean
clean :
	test -d 'pairctl' || rm pairctl
	test -d '$(OBJECTS)' || rm $(OBJECTS)
