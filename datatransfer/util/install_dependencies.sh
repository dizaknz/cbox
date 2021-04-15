#!/bin/bash

util_dir="$(dirname $0)"
dir=$util_dir/..

set -e

# get a recent CPAN version
cpan CPAN

# install needed modules
perl -MCPAN -I"$dir/lib" -e "install Bundle::Config"
