#! /bin/bash

BIN=$(cd $(dirname $0) && pwd)

mpirun -np 2 -num-cores 1 -machinefile $BIN/machinefile.pinto $BIN/test
