# dir-tree

A directory tree lister for Windows (and Linux) modeled on the Unix `tree` utility.

Uses C++17 <filesystem>. Draws the classic box art with Unicode line-drawing characters
and prints a directory/file summary.

## Build 

Build (MSVC):   `cl /EHsc /std:c++17 /utf-8 dir-tree.cpp`
Build (MinGW):  `g++ -std=c++17 -O2 dir-tree.cpp -o dir-tree.exe`
Build (Linux):  `g++ -std=c++17 -O2 dir-tree.cpp -o dir-tree`

## Usage

```
Usage: dir-tree [options] [directory]
  -a          Include hidden files (dotfiles and Windows "hidden" attribute)
  -d          List directories only
  -f          Print the full path prefix for each entry
  -L <level>  Descend only <level> directories deep
  -n          Turn off colorization
  -h, --help  Show help
```
