
TARGET = xlsxconverter

CC = clang
CXX = clang++
ifeq ($(OS),Windows_NT)
  CC = gcc
  CXX = g++
endif

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)
LIBS = external/libzip.a external/libpugixml.a external/libyaml-cpp.a
HEADERS = $(wildcard *.hpp) $(wildcard handlers/*.hpp) $(wildcard utils/*.hpp)

CPPFLAGS = -std=c++11 -O3 -I. -I./external -I./external/ziplib/Source/ZipLib -I./external/pugixml -I./external/yaml-cpp/include
ifneq ($(DEBUG),)
	CPPFLAGS:=-g $(CPPFLAGS) -DDEBUG=$(DEBUG)
endif

LDFLAGS = -L./external -lzip -lpugixml -lyaml-cpp

TEST = ./$(TARGET) --jobs full \
		--xls_search_path test --yaml_search_path test --output_base_path test \
		--timezone +0900 \
		dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml

all: $(TARGET)

main.o: main.cpp $(HEADERS) Makefile

external/libzip.a:
	cd external/ziplib && make CC=$(CC) CXX=$(CXX)
	cp external/ziplib/Bin/libzip.a external/libzip.a

external/libpugixml.a:
	cd external/pugixml && $(CXX) $(CPPFLAGS) -c pugixml.cpp -o pugixml.o
	cd external/pugixml && $(AR) r ../libpugixml.a pugixml.o

external/libyaml-cpp.a:
	cd external/yaml-cpp && cmake .
	cd external/yaml-cpp && make
	cd external/yaml-cpp && cp libyaml-cpp.a ..

.cpp.o:
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBS)
	$(CXX) $(CPPFLAGS) $< $(LDFLAGS) -o $@


test: $(TARGET)
	ulimit -c unlimited && ($(TEST) || (lldb -c `ls -t /cores/* | head -n1` --batch -o 'thread backtrace all' -o 'quit' && exit 1))
	[ -e ../test.sh ] && ../test.sh

clean:
	cd external/pugixml && $(RM) *.o
	cd external/ziplib && make clean
	cd external/ziplib && $(RM) Bin/libzip.a
	cd external/yaml-cpp && make clean
	cd external/yaml-cpp && ./clean.sh
	$(RM) $(TARGET) $(OBJS) $(LIBS)
