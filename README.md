# xlsxconverter

xlsx to data converter written in c++11.
[ssc](https://github.com/yamionp/spreadsheetconverter/) compatible.

[![Build Status](https://travis-ci.org/peposso/xlsxconverter.svg?branch=master)](https://travis-ci.org/peposso/xlsxconverter)
[![Build status](https://ci.appveyor.com/api/projects/status/mh6nqcgibro2nvho?svg=true)](https://ci.appveyor.com/project/peposso/xlsxconverter)

## USAGE

    xlsxconverter [--quiet]
                  [--jobs <'full'|'half'|'quarter'|int>]
                  [--xls_search_path <path>]
                  [--yaml_search_path <path>]
                  [--output_base_path <path>]
                  [--timezone <tz>]
                  [<target_yaml> ...]

## EXAMPLE

    $ tree
    .
    ├── bin
    │   └── xlsxconverter
    ├── xls
    │   └── postalcode.xlsx
    └── yaml
        └── postalcode.yml

    $ ./bin/xlsxconverter --yaml_search_path yaml --xls_search_path xls --output_base_path json
    output: json/postalcode.json

    $ tree
    .
    ├── bin
    │   └── xlsxconverter
    ├── json
    │   └── postalcode.json
    ├── xls
    │   └── postalcode.xlsx
    └── yaml
        └── postalcode.yml

## YAML FORMAT

| key                          | type | desc |
| ---------------------------- | ---- | ---- |
| target                       | str  | "xls:///(xlsx_path)#(sheet_name)" <br> using wildcard, inputs as merged xlss. |
| row                          | int  | row number of column name |
| handler.path                 | str  | output file path |
| handler.type                 | str  | output file type (json,djangofixture,csv,lua,template) |
| handler.indent               | int  | indentation spaces (in json,lua) |
| handler.sort_keys            | bool | with sorted keys (in json,lua) |
| handler.comment_row          | int  | with comment line (in csv) |
| handler.csv_field_type       | int  | with type name line (in csv) |
| handler.csv_field_column     | int  | with column name line (in csv) |
| handler.source               | str  | template source path (in template) |
| handler.context              | map  | additional context (in template) |
| fields[].column              | str  | output field name |
| fields[].name                | str  | input field name |
| fields[].type                | str  | field type (int,float,char,bool,foreignkey,datetime,unixtime,isignored) |
| fields[].type_alias          | str  | field type name (in csv) |
| fields[].default             | any  | field default value |
| fields[].optional            | bool | no except unless xls column |
| fields[].relation.from       | str  | relational yaml path |
| fields[].relation.column     | str  | select $column from $from when $key = cell.value; |
| fields[].relation.key        | str  |  |
| fields[].relation.ignore     | int  |  |
| fields[].definition          | map  | mapping definition |
| fields[].validate.unique     | bool | checking if unique |
| fields[].validate.sorted     | bool | checking if sorted |
| fields[].validate.sequential | bool | checking if sequential |
| fields[].validate.min        | int  | checking if >= $min |
| fields[].validate.max        | int  | checking if <= $max |
| fields[].validate.anyof      | seq  | checking if value is any of $anyof |

## EXTERNAL LIBRARIES

all in repo.

- boost (Boost Software License) [http://www.boost.org/]
- yaml-cpp (MIT License) [https://github.com/jbeder/yaml-cpp]
- pugixml (MIT License) [http://pugixml.org/]
- ziplib (zlib License) [https://bitbucket.org/wbenny/ziplib/wiki/Home]
- Mustache (MIT License) [https://github.com/kainjow/Mustache]


## BUILD

[Latest Builds](https://github.com/peposso/xlsxconverter/releases)

### mac

    $ brew install cmake
    $ make

### linux

    $ docker run -v /path/to/xlsxconverter:/storage -it ubuntu /bin/bash
    # cd /storage
    # apt-get install build-essential cmake
    # make

### windows

    for x86_64 (on msys2-mingw64)
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S mingw-w64-x86_64-cmake
    $ pacman -S make
    $ cd path/to/xlsxconverter
    $ make

    for x86 (on msys2-mingw32)
    $ pacman -S mingw-w64-i686-toolchain
    $ pacman -S mingw-w64-i686-cmake
    $ pacman -S make
    $ cd path/to/xlsxconverter
    $ make


## LICENSE

    Copyright 2016 peposso

    Permission is hereby granted, free of charge, to any person or organization
    obtaining a copy of the software and accompanying documentation covered by
    this license (the "Software") to use, reproduce, display, distribute,
    execute, and transmit the Software, and to prepare derivative works of the
    Software, and to permit third-parties to whom the Software is furnished to
    do so, all subject to the following:

    The copyright notices in the Software and this entire statement, including
    the above license grant, this restriction and the following disclaimer,
    must be included in all copies of the Software, in whole or in part, and
    all derivative works of the Software, unless such copies or derivative
    works are solely in the form of machine-executable object code generated by
    a source language processor.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
    SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
    FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

