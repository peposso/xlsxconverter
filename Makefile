
TARGET = xlsxconverter

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)
LIBS = external/libyaml-cpp.a external/libzip.a external/libpugixml.a
HEADERS = $(wildcard *.hpp) $(wildcard handlers/*.hpp) $(wildcard utils/*.hpp)

CPPFLAGS = -std=c++11 -O3 -I. -I./external -I./external/ziplib/Source/ZipLib -I./external/pugixml -I./external/yaml-cpp/include

LDFLAGS = -L./external -lzip -lpugixml -lyaml-cpp -lpthread
ifneq ($(filter $(MSYSTEM),MINGW64 MINGW32),)
	EXTRA_LDFLAGS = -static -static-libgcc -static-libstdc++
	EXE = .exe
else
	EXTRA_LDFLAGS =
	EXE =
endif

ifneq ($(DEBUG),)
	CPPFLAGS += -g -fsanitize=address -fstack-protector-all -fno-omit-frame-pointer $(CPPFLAGS) -DDEBUG=$(DEBUG)
	# CPPFLAGS:=-g -fsanitize=thread -fstack-protector-all -fno-omit-frame-pointer $(CPPFLAGS) -DDEBUG=$(DEBUG)
	LDFLAGS += -fsanitize=address
endif

TEST = ./$(TARGET)$(EXE) --jobs full \
		--xls_search_path test --yaml_search_path test --output_base_path test \
		--timezone '+0900' \
		dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml countrytmpl.yaml

ifeq ($(shell uname -s),Darwin)
	OS = mac
else ifeq ($(shell uname -s),Linux)
	OS = linux
else ifneq ($(filter $(MSYSTEM),MINGW64 MINGW32),)
	OS = windows
else
	OS = other
endif
ARCH = $(shell uname -m)

RELEASE_NAME = $(TARGET)-$(OS)-$(ARCH)-$(shell git rev-parse --short HEAD)


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
	cd external/yaml-cpp && cmake . -G "MSYS Makefiles"
else
	cd external/yaml-cpp && cmake .
endif
	cd external/yaml-cpp && $(MAKE) yaml-cpp/fast
	cd external/yaml-cpp && cp libyaml-cpp.a ..

.cpp.o:
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(TARGET): $(LIBS) $(OBJS)
	$(CXX) $(OBJS) $(EXTRA_LDFLAGS) $(LDFLAGS) -o $@

test-duplicate:
	# duplicate symbol test
	$(CXX) $(CPPFLAGS) -c test/test_link.cpp -o test_link1.o
	$(CXX) $(CPPFLAGS) -DMAIN -c test/test_link.cpp -o test_link2.o
	$(CXX) test_link1.o test_link2.o $(LDFLAGS) -o test_link
	$(RM) test_link

test:
	which ldd 2> /dev/null && ldd $(TARGET) || true
	which otool 2> /dev/null && otool -L $(TARGET) || true
ifeq ($(CC),clang)
	[ -e /cores ] && ulimit -c unlimited && ($(TEST) || (lldb -c `ls -t /cores/* | head -n1` --batch -o 'thread backtrace all' -o 'quit' && exit 1)) || $(TEST)
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
	which 7z 2> /dev/null && 7z a $(RELEASE_NAME).zip $(TARGET)$(EXE) || zip $(RELEASE_NAME).zip $(TARGET)$(EXE)
	mkdir build
	mv $(RELEASE_NAME).zip build
.PHONY: release
