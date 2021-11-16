#!/bin/bash

# Values of the 5 parameters. Generates outputs of all the combinations of parameters.
ki=(1 4)
km=(1 1.5)
kn=(0.5 1)
kf=(0.1 0.3)
ps=(0.01 0.0001)

# Number of segments to be sent
T=1000

# Number of combinations
((n = ${#ki[@]} * ${#km[@]} * ${#kn[@]} * ${#kf[@]} * ${#ps[@]})) 

# index variable
idx=1

echo "Using number of segments T = $T..."

for t1 in ${ki[@]}; do
    for t2 in ${km[@]}; do
        for t3 in ${kn[@]}; do
            for t4 in ${kf[@]}; do
                for t5 in ${ps[@]}; do
                    ./cw -i $t1 -m $t2 -n $t3 -f $t4 -s $t5 -T $T -o t${idx}.txt
                    python3 plot.py $idx $t1 $t2 $t3 $t4 $t5 
                    echo -ne "Progress:\t${idx}/${n}\r"
                    ((idx=idx+1))
                done
            done
        done
    done
done
echo "All testcase outputs captured."
echo "All graphs plotted."