#!/usr/bin/env bash

make
input_dir=./public-testcases/input
output_dir=./public-testcases/output
inputs=()
outputs=()
for entry in "$input_dir"/*;
do
    inputs+=("$entry")
done
for entry in "$output_dir"/*;
do
    outputs+=("$entry")
done
for ((i=0; i<"${#inputs[@]}"; i++));
do
    ./friend Not_Tako < ${inputs[$i]} > ./output
    diff_output=$(diff -b ./output ${outputs[$i]})
    if [[ -z "$diff_output" ]];
    then
        echo -e "\033[32m ${inputs[$i]}\033[0m"
    else
        echo -e "\033[31m ${inputs[$i]}\033[0m"
        cat ${inputs[$i]}
        echo "--------------------"
        echo "your output:"
        cat output
        echo "--------------------"
        echo "answer:"
        cat ${outputs[$i]}
        echo "--------------------"
        echo "difference:"
        echo "$diff_output"
        make clean
        exit 0
    fi
done
make clean