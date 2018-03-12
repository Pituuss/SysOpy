CC = gcc -Wall
LIB_NAME = lib
MAIN_SRC = main

compile: static shared dynamic

static:
	# kompilacja zrodla
	$(CC) -c $(LIB_NAME).c -o $(LIB_NAME).o -O$(optim)

	# zbuduj biblioteke statyczna .a
	ar rcs lib$(LIB_NAME).a $(LIB_NAME).o 

	# linkowanie
	$(CC) -static $(MAIN_SRC).c -L. -l $(LIB_NAME) -o $(MAIN_SRC)_static -O$(optim)

shared:
	# zbuduj biblioteke wspoldzielona .so
	$(CC) -fPIC -c $(LIB_NAME).c -o $(LIB_NAME).o -O$(optim)
	$(CC) -fPIC -shared -o lib$(LIB_NAME).so $(LIB_NAME).o -O$(optim)

	# linkowanie
	$(CC) -Wl,-rpath=. -L. -o $(MAIN_SRC)_shared $(MAIN_SRC).c -l$(LIB_NAME) -O$(optim)

dynamic:
	# zbuduj biblioteke wspoldzielona .so
	$(CC) -fPIC -c $(LIB_NAME).c -o $(LIB_NAME).o -O$(optim)
	$(CC) -Wl,-rpath=. -fPIC -shared -o lib$(LIB_NAME).so $(LIB_NAME).o -O$(optim)

	# linkowanie
	$(CC) -L. -o $(MAIN_SRC)_dynamic $(MAIN_SRC).c -ldl -O$(optim) -D DYNAMIC 

clean:
	rm -f *~
	rm -f *.o
	rm -f *.a
	rm -f *.so

clean_all: clean
	rm -f $(MAIN_SRC)_dynamic
	rm -f $(MAIN_SRC)_shared
	rm -f $(MAIN_SRC)_static

test: compile
	./$(MAIN_SRC)_static 500000 500 dynamic f 100 ar 100000
	./$(MAIN_SRC)_static 500000 500 static f 100 ar 100000
	./$(MAIN_SRC)_shared 500000 500 dynamic f 100 ar 100000
	./$(MAIN_SRC)_shared 500000 500 static f 100 ar 100000
	./$(MAIN_SRC)_dynamic 500000 500 dynamic f 100 ar 100000
	./$(MAIN_SRC)_dynamic 500000 500 static f 100 ar 100000
	./$(MAIN_SRC)_static 100000 500 dynamic ar 10000 arc 10000
	./$(MAIN_SRC)_static 100000 500 static ar 10000 arc 10000
	./$(MAIN_SRC)_shared 100000 500 dynamic ar 10000 arc 10000
	./$(MAIN_SRC)_shared 100000 500 static ar 10000 arc 10000
	./$(MAIN_SRC)_dynamic 100000 500 dynamic ar 10000 arc 10000
	./$(MAIN_SRC)_dynamic 100000 500 static ar 10000 arc 10000
make clean_all