# Define required macros here
SHELL = /bin/sh
CXX=g++
CC=gcc
CXXFLAGS= -std=c++11 -c -Wall -pedantic -Werror -O2 -DNDEBUG -DINTEL_SCALABLE_ALLOCATOR -DCONFIG_USE_EPOLL
CXXFLAGSD= -std=c++11 -c -Wall -pedantic -Werror -DDEBUG -DCONFIG_USE_EPOLL -ggdb
CFLAGS= -std=c99 -c -Wall -pedantic -Werror -O2 -DNDEBUG -DINTEL_SCALABLE_ALLOCATOR -DCONFIG_USE_EPOLL
CFLAGSD= -std=c99 -c -Wall -c -Werror -DDEBUG -DCONFIG_USE_EPOLL -ggdb
INC=-I. $(shell python-config --includes)
LIB=-ltbb -ltbbmalloc -lz $(shell python-config --libs)
CXX_SRC_CONSOLE = $(shell find ./src/console/ -name *.cpp)
CXX_SRC_SHARED = $(shell find ./src/shared/ -name *.cpp | grep -v "StackWalker.cpp")
CC_SRC_CONSOLE = $(shell find ./src/console/ -name *.c)
CC_SRC_SHARED = $(shell find ./src/shared/ -name *.c | grep -v "zlib")
CXX_SRC = $(CXX_SRC_SHARED) $(CXX_SRC_CONSOLE)
CC_SRC = $(CC_SRC_CONSOLE) $(CC_SRC_SHARED)
OBJCXX=$(CXX_SRC:.cpp=.o)
OBJCXXD=$(CXX_SRC:.cpp=.od)
OBJCC=$(CC_SRC:.c=.o)
OBJCCD=$(CC_SRC:.c=.od)
EXE=transdb
EXED=transdb_d

all: debug release

release: $(CC_SRC_SHARED) $(CC_SRC_CONSOLE) $(CXX_SRC_SHARED) $(CXX_SRC_CONSOLE) $(EXE)
	cp $(EXE) ../

debug: $(SRC_SHARED) $(SRC_CONSOLE) ${EXED}
	cp $(EXED) ../

$(EXE): $(OBJCC) $(OBJCXX)
	$(CXX) $(OBJCXX) $(OBJCC) $(LIB) -s -o $@ 

$(EXED): $(OBJCCD) $(OBJCXXD)
	$(CXX) $(OBJCXXD) $(OBJCCD) $(LIB) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) ${INC} $< -o $@

%.od: %.cpp
	$(CXX) $(CXXFLAGSD) ${INC} $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.od: %.c
	$(CC) $(CFLAGSD) $< -o $@

.PHONY: clean
clean:
	find . -type f -name *.od -o -name *.o -o -name *.co | xargs rm  
	rm -f ${EXE} ${EXED}
