
TARGET = xlsxconverter

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:%.cpp=%.o)
LIBS = external/libyaml-cpp.a external/libzip.a external/libpugixml.a
HEADERS = $(wildcard src/*.hpp) $(wildcard src/handlers/*.hpp) $(wildcard src/utils/*.hpp)

CPPFLAGS = -std=c++11 -O3 -I./src -I./external -I./external/ziplib/Source/ZipLib -I./external/pugixml -I./external/yaml-cpp/include

LDFLAGS = -L./external -lzip -lpugixml -lyaml-cpp -lpthread
ifneq ($(filter $(MSYSTEM),MINGW64 MINGW32),)
	EXTRA_LDFLAGS = -static -static-libgcc -static-libstdc++
	EXE = .exe
else
	EXTRA_LDFLAGS =
	EXE =
endif

ifneq ($(DEBUG),)
	CPPFLAGS += -O0 -g3 -fstack-protector-all -DDEBUG=$(DEBUG)
	# CPPFLAGS:=-g -fsanitize=thread -fstack-protector-all -fno-omit-frame-pointer $(CPPFLAGS) -DDEBUG=$(DEBUG)
	LDFLAGS += -fstack-protector-all
endif

TEST = ./$(TARGET)$(EXE) --jobs full \
		--xls_search_path test --yaml_search_path test --output_base_path test --timezone '+0900'

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

IS_GCC = $(shell $(CC) -v 2>&1 | grep gcc | head -n1)
IS_CLANG = $(shell $(CC) -v 2>&1 | grep clang | head -n1)

DEBUGGER =
ifneq ($(IS_CLANG),)
ifneq ($(shell which lldb 2>/dev/null),)
	DEBUGGER = lldb -k --batch -o "run" -o "thread backtrace all" -o "quit" --
endif
endif
ifneq ($(IS_GCC),)
ifneq ($(shell which gdb 2>/dev/null),)
	DEBUGGER = gdb -batch -ex "run" -ex "thread apply all backtrace" -ex "quit" --args
endif
endif

LDD = ldd
ifeq ($(OS),mac)
	LDD = otool -L
endif

all: $(TARGET)

src/main.o: src/main.cpp $(HEADERS) Makefile

external/libzip.a:
	cd external/ziplib && $(MAKE) CC=$(CC) CXX=$(CXX)
	cp external/ziplib/Bin/libzip.a external/libzip.a

external/libpugixml.a:
	cd external/pugixml && $(CXX) $(CPPFLAGS) -c pugixml.cpp -o pugixml.o
	cd external/pugixml && $(AR) r ../libpugixml.a pugixml.o

external/libyaml-cpp.a:
ifneq ($(filter $(MSYSTEM),MINGW64 MINGW32),)
	cd external/yaml-cpp && CC=$(CC) CXX=$(CXX) cmake . -G "MSYS Makefiles"
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

test-util:
	$(CXX) $(CPPFLAGS) -O0 -g3 test/test_fs.cpp -o test-util.exe
	$(DEBUGGER) ./test-util.exe
	-rm test-util.exe

test-xlsx:
	$(CXX) $(CPPFLAGS) -O0 -g3 test/test_xlsx.cpp $(LDFLAGS) -o test_xlsx.exe
	$(DEBUGGER) ./test_xlsx.exe
	-rm test_xlsx.exe

test: $(TARGET)
	-./external/cpplint.py --linelength=100 --filter=-build/c++11 --extensions=hpp,cpp src/**/*.hpp src/**.hpp src/**.cpp
	$(LDD) $(TARGET)
	$(DEBUGGER) $(TEST)
	python test/check_json.py test/sample.json
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
	-mkdir build
	mv $(RELEASE_NAME).zip build
.PHONY: release
