#!/bin/sh

cd `dirname $0`/..

ARGS1='--xls_search_path test --yaml_search_path test --json_base_path test'
ARGS2='--xls_search_path test --yaml_search_path test --output_base_path test --quiet'
TARGETS='dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml'


echo "--------------------------------"
echo "python ssconvert..."
time for i in {1..10}; do
    ssconvert ${ARGS1} ${TARGETS} 1>/dev/null 2>/dev/null
done

echo "--------------------------------"
echo "c++ xlsxconverter... jobs 4"
time for i in {1..10}; do
    ./xlsxconverter --jobs 4 ${ARGS2} ${TARGETS}
done

echo "--------------------------------"
echo "c++ xlsxconverter... jobs 2"
time for i in {1..10}; do
    ./xlsxconverter --jobs 2 ${ARGS2} ${TARGETS}
done

echo "--------------------------------"
echo "c++ xlsxconverter... jobs 1"
time for i in {1..10}; do
    ./xlsxconverter --jobs 1 ${ARGS2} ${TARGETS}
done

