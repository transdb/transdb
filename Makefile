# Define required macros here
SHELL = /bin/sh
CC=g++
CFLAGS= -std=c++11 -c -Wall -O3 -DNDEBUG
CFLAGSD= -std=c++11 -c -Wall -DDEBUG -ggdb
INC=-I. $(shell python-config --includes)
LIB=-ltbb -ltbbmalloc -lz $(shell python-config --libs)
SRC_CONSOLE = $(shell find ./src/console/ -name *.cpp) 
SRC_SHARED = $(shell find ./src/shared/ -name *.cpp | grep -v "StackWalker.cpp") 
SRC= $(SRC_SHARED) $(SRC_CONSOLE) 
OBJ=$(SRC:.cpp=.o)
OBJD=$(SRC:.cpp=.od)
EXE=transdb
EXED=transdb_d

all: debug release

release: $(SRC_SHARED) $(SRC_CONSOLE) $(EXE)
	cp $(EXE) ../

debug: $(SRC_SHARED) $(SRC_CONSOLE) ${EXED}
	cp $(EXED) ../

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LIB) -s -o $@ 

$(EXED): $(OBJD)
	$(CC) $(OBJD) $(LIB) -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) ${INC} $< -o $@

%.od: %.cpp
	$(CC) $(CFLAGSD) ${INC} $< -o $@

.PHONY: clean
clean:
	find . -type f -name *.od -o -name *.o | xargs rm  
	rm -f ${EXE} ${EXED}
