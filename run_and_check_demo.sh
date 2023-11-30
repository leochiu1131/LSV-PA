#/bin/bash

for i in $(seq 0 100)
do
    ./abc -f "./test_aig${i}.src" > "./test_aig${i}.log"
    ./abc -f "./test${i}.src" > "./test${i}.log"
done

# Second loop: Execute the diff commands
for i in $(seq 0 100)
do
    diff "./test_aig${i}.log" "./test${i}.log" >> ./check.log
done