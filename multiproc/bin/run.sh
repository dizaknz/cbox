#! /bin/bash

BIN=$(cd $(dirname $0) && pwd)

NP=4
[ $# -eq 1 ] && NP=$1

echo "Running with $NP processes"
time mpirun -np $NP -num-cores 1 -machinefile $BIN/machinefile.pinto $BIN/multiproc
