# absolute path to this
BIN=`cd $(dirname $0); pwd`

[ -n "$1" -a "$1" = "-d" ] && DEBUG=valgrind

make clean && make
[ $? -ne 0 ] && { echo "FATAL: could not build demo"; exit 1; }

echo "INFO: Running bin/crcSearch.pl"
$BIN/bin/crcSearch.pl --file $BIN/bin/C_task.docx
echo

echo "INFO: Running bin/crcsearch with 0x42dc2cec8897c034"
$DEBUG $BIN/bin/crcsearch -i $BIN/bin/C_task.docx -q 0x42dc2cec8897c034
echo

echo "INFO: Running bin/crcsearch with 0xf1d682970a98cee"
$DEBUG $BIN/bin/crcsearch -i $BIN/bin/C_task.docx -q 0xf1d682970a98cee
echo "INFO: Done"
