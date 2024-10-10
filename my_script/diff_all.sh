#!/bin/bash

A_RESULT_DIR="$HOME/LSV-PA/my_result/"
MY_RESULT="$HOME/LSV-PA/WSY_result/"
# MY_RESULT="$HOME/LSV-PA/YHH_result/"
# MY_RESULT="$HOME/LSV-PA/bob_result/"
# MY_RESULT="/mnt/data/WSY/WSY_result/"

MAPPER="$HOME/LSV-PA/my_script/mapper.py"

BENCHMARKS='comp adder div int2float log2 mem_ctrl router sqrt square'

for case in ${BENCHMARKS}
do
    for i in {3..6}; do
        echo -e "\033[0;32m Compare ${case}, cut ${i}\033[0m"
        python ${MAPPER} ${A_RESULT_DIR}${case}${i}.out ${MY_RESULT}${case}${i}.out
        printf "\n\n"
    done
done
