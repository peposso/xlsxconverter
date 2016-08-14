#!/bin/sh

cd `dirname $0`

rm output/dummy1.json
mkdir output

echo "--------------------------------"
echo "python ssconvert..."
time for i in {1..30}; do
    ssconvert dummy1.yaml --xls_search_path test --yaml_search_path test --json_base_path output 1>/dev/null 2>/dev/null
    rm output/dummy1.json
done

echo "--------------------------------"
echo "c++ xlsxconvert... jobs 4"
time for i in {1..100}; do
    ./xlsxconverter --jobs 4 dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml --xls_search_path test --yaml_search_path test --output_base_path output --quiet
    rm output/dummy1.json
done

echo "--------------------------------"
echo "c++ xlsxconvert... jobs 2"
time for i in {1..100}; do
    ./xlsxconverter --jobs 2 dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml --xls_search_path test --yaml_search_path test --output_base_path output --quiet
    rm output/dummy1.json
done

echo "--------------------------------"
echo "c++ xlsxconvert... jobs 1"
time for i in {1..100}; do
    ./xlsxconverter --jobs 1 dummy1.yaml dummy1fix.yaml dummy1csv.yaml dummy1lua.yaml --xls_search_path test --yaml_search_path test --output_base_path output --quiet
    rm output/dummy1.json
done

