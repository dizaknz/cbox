#! /bin/bash
#
# test script
#

# absolute path to this
BIN=`cd $(dirname $0); pwd`
readonly SERVER_STORAGE=/tmp/server
readonly CLIENT1_STORAGE=/tmp/client1
readonly CLIENT2_STORAGE=/tmp/client2
# 1K block counts read from urandom
readonly FILE_BLOCK_COUNT=5000

readonly CLIENTS=($CLIENT1_STORAGE $CLIENT2_STORAGE);

usage () {
    cat <<EOF
NAME
       $(basename $0)

SYNOPSIS
       $(basename $0) -n <numfiles> -c <numclients> -d

DESCRIPTION
       -n    number of random test files to serve to clients
       -c    number of clients to connect to the server
       -d    flag to enable valgrind checking for debugging
EOF
}


transferSingleFile () {
    transferfile=$1

    [ -s $SERVER_STORAGE/$transferfile ] || { 
        echo "Error - invalid file: $transferfile to transfer to clients"
        return 1
    }

    # server filter checks filename that start with filter name, eg. file1 or
    # can be forced to server single file only
    # when no filter is supplied it serves all its files
    echo "Starting server"
    $WRAPPER $BIN/fileserver -p 5008 -s $SERVER_STORAGE -f $transferfile &> $BIN/server.log &
    pid=$!

    sleep 5

    # start up some clients
    # let them fetch the files
    sleep 1 && $WRAPPER $BIN/fileclient -i 127.0.0.1 -p 5008 -s $CLIENT1_STORAGE &
    $WRAPPER $BIN/fileclient -i 127.0.0.1 -p 5008 -s $CLIENT2_STORAGE

    # todo - not implemented
    # now modify some files by overwriting some random content with some other random content

    sleep 5

    echo "Closing server"
    kill -SIGINT $pid

    # check that files have been received correctly
    for client in ${CLIENTS[*]}; do
        diff $SERVER_STORAGE/$transferfile $client/$transferfile
        [ $? -eq 0 ] && {
            echo "Successfully transferred server file: $transferfile to client: $client"
        } || {
            echo "Error transferring server file: $transferfile to client: $client"
        }

    done

    return $?
}

main () {
    numfiles=1

    while getopts "n:c:d" opt; do
        case $opt in
            n):
                numfiles=$OPTARG
                ;;
            c):
                numclients=$OPTARG
                ;;
            d):
                WRAPPER="valgrind --leak-check=full"
                ;;
            h):
                usage
                exit 0
                ;;
            *):
                ERROR "$opt is an invalid option"
                usage
                exit 1
                ;;
        esac
    done

    echo "Populating storage"
    [ -d $SERVER_STORAGE ] || mkdir -p $SERVER_STORAGE
    [ $? -ne 0 ] && {
        echo "Failed to create $SERVER_STORAGE"
        return 1
    }

    for CLIENT_STORAGE in ${CLIENTS[*]}; do
        [ -d $CLIENT_STORAGE ] || mkdir -p $CLIENT_STORAGE
        [ $? -ne 0 ] && {
            echo "Failed to create $CLIENT_STORAGE"
            return 1
        }
    done
    
    # create $numfiles of 1k * FILE_BLOCK_COUNT size files with random content
    for i in $(seq 1 $numfiles); do
        filename=$SERVER_STORAGE/file${i}.dat
        [ -s $filename ] || {
            echo "Creating file: $filename"
            dd if=/dev/urandom bs=1024 count=$FILE_BLOCK_COUNT of=$filename
        }
    done

    [ -s $BIN/fileserver -a -s $BIN/fileclient ] || {
        (
          echo "Building project"
          cd $BIN/..
          make clean
          make
        )
    }

    for f in $SERVER_STORAGE/file*.dat; do
        file=$(basename $f);

        echo "Info - serving file: $file in single transfer mode"
        transferSingleFile $file
    done

    return $?
}

main $@
exit $?
