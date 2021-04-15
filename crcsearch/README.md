# crcsearch

Job interview question circa 2016

# Run

Run run.sh

```
.
├── bin
│   ├── crcsearch
│   ├── crcSearch.pl
│   └── C_task.docx
├── Makefile
├── README.txt
└── src
    ├── crcsearch.c
    ├── crcsearch.h
    └── crcsearch.o
```

Perl script crcSearch.pl written to verify C program using Digest::CRC, see

```
bin/crcSearch.pl -h
Usage:
    crcSearch.pl -f <file> -q <CRC checksum> [-d -h]

Arguments:
      --file    Binary file that CRC checksum orginated from
      --crc     Search for given checksum, hex
      --debug   Enable debug messages
      --help    Brief help message
```


```
make clean && make to build demo C program
```

```
bin/crcsearch

NAME                                           
       crcsearch                                                        
                                                                        
SYNOPSIS                                                                
       crcsearch -i file -q crc checksum                                
                                                                        
DESCRIPTION                                                             
       Searches file for a given checksum and report it's range in file 
```

