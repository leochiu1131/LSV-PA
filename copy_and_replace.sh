#!/bin/bash

# Loop from 1 to 98
for i in {0..98}
do
    # Copy the file and replace '99' with the current number
    sed 's/99/'"$i"'/g' test_aig99.src > "test_aig${i}.src"
done
