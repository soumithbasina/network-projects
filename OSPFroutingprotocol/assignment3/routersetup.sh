#!/bin/bash

# trap SIGINT as a signal to kill the processes 
trap "kill 0" SIGINT

# values of all the parameters
hi=1
li=5
si=10

# number of routers, passed as a command line argument
n=$1

# name of the infile passed as a command line argument
infile=$2

# execute all the processes in parallel
id=0
while [ $id -lt $n ]
do
    ./ospf -i $id -f $infile -o outfile -h $hi -a $li -s $si &
    ((id=id+1))
done

# wait until ctrl + C is passed
wait