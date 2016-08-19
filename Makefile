
TARGET = xlsxconverter

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)
LIBS = external/libzip.a external/libpugixml.a external/libyaml-cpp.a
HEADERS = $(wildcard *.hpp) $(wildcard handlers/*.hpp) $(wildcard utils/*.hpp)

CPPFLAGS = -std=c++11 -O3 -I. -I./external -I./external/ziplib/Source/ZipLib -I./external/pugixml -I./external/yaml-cpp/include
ifneq ($(DEBUG),)
	CPPFLAGS:=-g $(CPPFLAGS) -DDEBUG=$(DEBUG)
endif

LDFLAGS = -L./external -lzip -lpugixml -lyaml-cpp -lpthread

TEST = ./$(TARGET) --jobs full \
		--xls_search_path test --yaml_search_path test --output_base_path test \
		--timezone +0900 \
		dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml countrytmpl.yaml

ifeq ($(shell uname -s),Darwin)
	OS = mac
else ifeq ($(shell uname -s),Linux)
	OS = linux
else
	OS = windows
endif

RELEASE_NAME = $(TARGET)-$(OS)-$(shell uname -m)-$(shell git rev-parse --short HEAD)


all: $(TARGET)

main.o: main.cpp $(HEADERS) Makefile

external/libzip.a:
	cd external/ziplib && $(MAKE) CC=$(CC) CXX=$(CXX)
	cp external/ziplib/Bin/libzip.a external/libzip.a

external/libpugixml.a:
	cd external/pugixml && $(CXX) $(CPPFLAGS) -c pugixml.cpp -o pugixml.o
	cd external/pugixml && $(AR) r ../libpugixml.a pugixml.o

external/libyaml-cpp.a:
ifneq ($(filter $(MSYSTEM),MINGW64 MINGW32),)
	cd external/yaml-cpp && CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) cmake . -G "MinGW Makefiles"
else
	cd external/yaml-cpp && cmake .
endif
	cd external/yaml-cpp && $(MAKE) yaml-cpp/fast
	cd external/yaml-cpp && cp libyaml-cpp.a ..

.cpp.o:
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBS)
	$(CXX) $(CPPFLAGS) $< $(LDFLAGS) -o $@

test-duplicate:
	# duplicate symbol test
	$(CXX) $(CPPFLAGS) -c test/test_link.cpp -o test_link1.o
	$(CXX) $(CPPFLAGS) -DMAIN -c test/test_link.cpp -o test_link2.o
	$(CXX) test_link1.o test_link2.o $(LDFLAGS) -o test_link
	$(RM) test_link

test:
ifneq ($(CC),clang)
	ulimit -c unlimited && ($(TEST) || (lldb -c `ls -t /cores/* | head -n1` --batch -o 'thread backtrace all' -o 'quit' && exit 1))
else
	$(TEST)
endif
	python test/check_json.py test/dummy1.json
	python test/check_json.py test/dummy1fix.json
	[ -e ../test.sh ] && ../test.sh || true
.PHONY: test

clean:
	cd external/pugixml && $(RM) *.o
	cd external/ziplib && $(MAKE) clean
	cd external/ziplib && $(RM) Bin/libzip.a
	# cd external/yaml-cpp && $(MAKE) clean
	cd external/yaml-cpp && ./clean.sh
	$(RM) $(TARGET) $(OBJS) $(LIBS)
.PHONY: clean

release: $(TARGET)
	-rm $(RELEASE_NAME).zip
	zip $(RELEASE_NAME).zip $(TARGET)
.PHONY: release
