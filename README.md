xlsx to data converter.


## build

cmake is required for yaml-cpp.

### mac

    $ brew install cmake
    $ make

### linux

    $ docker -v /path/to/xlsxconverter:/storage -it ubuntu /bin/bash
    # cd /storage
    # apt-get install build-essential cmake
    # make CC=gcc CXX=g++

### windows

    install msys2
    for x86_64
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S mingw-w64-x86_64-cmake
    $ cd path/to/xlsxconverter
    $ mingw32-make CC=gcc CXX=g++
    for x86
    $ pacman -S mingw-w64-i686-toolchain
    $ pacman -S mingw-w64-i686-cmake
    $ cd path/to/xlsxconverter
    $ mingw32-make CC=gcc CXX=g++
