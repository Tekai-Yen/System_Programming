#!/bin/sh

# usage "sh sample_execution.sh [1 / 2 / 3]"

# Sample 1
sample_1() {
    ./hw3 3 1 1 1 3 2 3 1 4 2 4 &
    child=$!
    sleep 0.5
    kill -TSTP $child
    sleep 2
    kill -TSTP $child
    sleep 3
    kill -TSTP $child
    wait $child
}

# Sample 2
sample_2() {
    ./hw3 3 5 6 3 3 2 8 2 3 2 3 2 1 &
    child=$!
    sleep 0.5
    kill -TSTP $child
    wait $child
}

# Sample 3
sample_3() {
    ./hw3 2 1 2 3 3 2 8 2 3 2 3 2 1 2 5 1 2 3 1 2 3 1 &
    child=$!
    sleep 0.5
    kill -TSTP $child
    sleep 5
    kill -TSTP $child
    sleep 1
    kill -TSTP $child
    sleep 2
    kill -TSTP $child
    wait $child
}

print_help() {
    echo "usage: $0 [subtask]"
}

main() {
    case "$1" in
    1)
        sample_1
        ;;
    2)
        sample_2
        ;;
    3)
        sample_3
        ;;
    *)
        print_help
        ;;
    esac
}

cleanup() {
    echo "Terminating all processes..."
    kill 0
    exit 0
}

trap cleanup INT

main "$1"
