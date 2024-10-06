#! /bin/bash

RESULT_DIR="$HOME/LSV-PA/my_result/"
SCRIPT_DIR="$HOME/LSV-PA/my_script/"
BENCHMARK_DIR="$HOME/LSV-PA/lsv/pa1/benchmarks/"

BENCHMARKS='adder div int2float log2 mem_ctrl router sqrt square'

for i in {3..6}; do
    # cat ${SCRIPT_DIR}Command.in | awk '{if ($1=="lsv_printcut") {$2='${i}'; print $0} else {if ($1 == "read") {$2="./comp.blif"; print $0} else print $0}}' > ./Command.in # also work
    cat ${SCRIPT_DIR}Command.in | awk '{if ($1=="lsv_printcut") {$2='${i}'} else {if ($1 == "read") {$2="./comp.blif"} } print $0'} > ./Command.in
    echo -e "\033[0;32m Run ./comp.blif, cut ${i}\033[0m"
    time ./abc < ./Command.in > ${RESULT_DIR}comp${i}.out
    printf "\n\n"
done


for case in ${BENCHMARKS}
do
    for i in {3..6}; do
        cat ${SCRIPT_DIR}Command.in | awk '{if ($1=="lsv_printcut") {$2='${i}'} else {if ($1 == "read") {$2="'${BENCHMARK_DIR}${case}.blif'"} } print $0'} > ./Command.in
        echo -e "\033[0;32m Run ${case}, cut ${i}\033[0m"
        time ./abc < ./Command.in > ${RESULT_DIR}${case}${i}.out
        printf "\n\n"
    done
done