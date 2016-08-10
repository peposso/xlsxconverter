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
echo "c++ xlsxconvert..."
time for i in {1..30}; do
    ./xlsxconverter dummy1.yaml --xls_search_path test --yaml_search_path test --output_base_path output
    rm output/dummy1.json
done

