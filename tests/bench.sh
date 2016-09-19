#!/bin/sh
set -e
cd `dirname $0`/..

ARGS1='--timezone +0900 --xls_search_path tests --yaml_search_path tests --json_base_path tests'
ARGS2='--timezone +0900 --xls_search_path tests --yaml_search_path tests --output_base_path tests --quiet'
TARGETS=''


# echo "--------------------------------"
# echo "python ssconvert..."
# time for i in {1..20}; do
#     ssconvert ${ARGS1} ${TARGETS} 1>/dev/null 2>/dev/null
# done

echo "--------------------------------"
echo "c++ xlsxconverter... --jobs 4 --no_cache"
time for i in {1..100}; do
    ./xlsxconverter --jobs 4 --no_cache ${ARGS2} ${TARGETS}
done

echo "--------------------------------"
echo "c++ xlsxconverter... --jobs 4"
time for i in {1..100}; do
    # lldb -k --batch -o "run" -o "thread backtrace all" -o "quit" -- ./xlsxconverter --jobs 4 ${ARGS2} ${TARGETS}
    ./xlsxconverter --jobs 4 ${ARGS2} ${TARGETS}
done

# echo "--------------------------------"
# echo "c++ xlsxconverter... jobs 2"
# time for i in {1..20}; do
#     ./xlsxconverter --jobs 2 ${ARGS2} ${TARGETS}
# done

echo "--------------------------------"
echo "c++ xlsxconverter... jobs 1"
time for i in {1..100}; do
    ./xlsxconverter --jobs 1 ${ARGS2} ${TARGETS}
done

echo

