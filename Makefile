CC = gcc -g
OBJECTS = insertpair.o
INCS  = -I /usr/local/BerkeleyDB.5.1/include
DEPS = -L /usr/local/BerkeleyDB.5.1/lib

insertpair : $(OBJECTS)
	$(CC) -o insertpair -ldb $(OBJECTS) $(DEPS)

insertpair.o: insertpair.c
	$(CC) -c insertpair.c $(INCS)

.PHONY : clean
clean :
	-rm insertpair $(OBJECTS)