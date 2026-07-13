// A directory tree lister for Windows (and Linux) modeled on the Unix `tree` utility. 
//
// Uses C++17 <filesystem>. Draws the classic box art with Unicode line-drawing characters
// and prints a directory/file summary.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

inline constexpr std::string_view AppName = "dir-tree";

namespace fs = std::filesystem;

static const char* BRANCH = "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 "; // "├── "
static const char* CORNER = "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "; // "└── "
static const char* VLINE  = "\xe2\x94\x82\x20\x20\x20";             // "│   "
static const char* BLANK  = "    ";

static const char* COL_DIR  = "\033[1;34m"; // bold blue for directories
static const char* COL_LINK = "\033[1;36m"; // bold cyan for symlinks
static const char* COL_EXE  = "\033[1;32m"; // bold green for executables
static const char* COL_OFF  = "\033[0m";

struct Options {
    bool showAll  = false; // -a
    bool dirsOnly = false; // -d
    bool fullPath = false; // -f
    bool color    = true;  // -n disables
    int  maxDepth = -1;    // -L (-1 == unlimited)
    std::string root = ".";
};

struct Counts {
    long long dirs  = 0;
    long long files = 0;
};

// Detect hidden entries: dotfiles on any platform, plus the Windows
// hidden/system attribute when compiled for Windows.
static bool isHidden(const fs::path& p) {
    const std::string name = p.filename().string();
    if (!name.empty() && name[0] == '.' && name != "." && name != "..") {
        return true;
    }
#ifdef _WIN32
    const DWORD attrs = GetFileAttributesW(p.wstring().c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
        return true;
    }
#endif
    return false;
}

// Check if directory entry is executable
static bool isExecutable(const fs::directory_entry& e) {
    std::error_code ec;
    const auto perms = e.status(ec).permissions();
    if (ec) {
        return false;
    }
    return (perms & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none;
}

// Case-insensitive comparison of the leaf names, matching `tree`'s default.
static bool nameLess(const fs::directory_entry& a, const fs::directory_entry& b) {
    std::string an = a.path().filename().string();
    std::string bn = b.path().filename().string();
    std::transform(an.begin(), an.end(), an.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(bn.begin(), bn.end(), bn.begin(), [](unsigned char c) { return std::tolower(c); });
    return an < bn;
}

static void printName(const fs::directory_entry& e, const Options& opt) {
    std::error_code ec;
    const bool isDir  = e.is_directory(ec);
    const bool isLink = e.is_symlink(ec);

    const std::string label = opt.fullPath 
        ? e.path().string() 
        : e.path().filename().string();

    if (opt.color) {
        if (isLink) {
            std::cout << COL_LINK << label << COL_OFF;
        }
        else if (isDir) {
            std::cout << COL_DIR  << label << COL_OFF;
        }
        else if (isExecutable(e)) {
            std::cout << COL_EXE << label << COL_OFF;
        }
        else {
            std::cout << label;
        }
    } else {
        std::cout << label;
    }

    if (isLink) {
        std::error_code lec;
        const fs::path target = fs::read_symlink(e.path(), lec);
        if (!lec) {
            std::cout << " -> " << target.string();
        }
    }
    std::cout << '\n';
}

static void walk(const fs::path& dir, const std::string& prefix, int depth, const Options& opt, Counts& counts) {
    if (opt.maxDepth >= 0 && depth > opt.maxDepth) return;

    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::directory_entry& e = *it;
        if (!opt.showAll && isHidden(e.path())) {
            continue;
        }
        std::error_code dec;
        const bool isDir = e.is_directory(dec);
        if (opt.dirsOnly && !isDir) {
            continue;
        }
        entries.push_back(e);
    }

    std::sort(entries.begin(), entries.end(), nameLess);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const fs::directory_entry& e = entries[i];
        const bool last = (i + 1 == entries.size());

        std::cout << prefix << (last ? CORNER : BRANCH);
        printName(e, opt);

        std::error_code dec;
        if (e.is_directory(dec) && !e.is_symlink(dec)) {
            ++counts.dirs;
            walk(e.path(), prefix + (last ? BLANK : VLINE), depth + 1, opt, counts);
        } else {
            ++counts.files;
        }
    }
}

static void printHelp() {
    std::cout <<
        "Usage: " << AppName << " [options] [directory]\n"
        "  -a          Include hidden files\n"
        "  -d          List directories only\n"
        "  -f          Print the full path prefix for each entry\n"
        "  -L <level>  Descend only <level> directories deep\n"
        "  -n          Turn off colorization\n"
        "  -h, --help  Show this help\n";
}

// Returns false if parsing should abort the program (bad args / help shown).
static bool parseArgs(int argc, char** argv, Options& opt, int& exitCode) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-a") {
            opt.showAll = true;
        }
        else if (arg == "-d") {
            opt.dirsOnly = true;
        }
        else if (arg == "-f") {
            opt.fullPath = true;
        }
        else if (arg == "-n") {
            opt.color = false;
        }
        else if (arg == "-L") {
            if (i + 1 >= argc) {
                std::cerr << AppName << ": option -L requires a level argument\n";
                exitCode = 1;
                return false;
            }
            try {
                opt.maxDepth = std::stoi(argv[++i]);
                if (opt.maxDepth < 1) throw std::out_of_range("");
            } catch (...) {
                std::cerr << AppName << ": invalid level " << argv[i] << " provided for -L\n";
                exitCode = 1;
                return false;
            }
        }
        else if (arg == "-h" || arg == "--help") { 
            printHelp(); 
            return false; 
        }
        else if (!arg.empty() && arg[0] == '-') {
            std::cerr << AppName << ": unknown option '" << arg << "'\n";
            exitCode = 1;
            return false;
        }
        else opt.root = arg; // treat as the directory to list
    }
    exitCode = 0;
    return true;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Render UTF-8 box characters and enable ANSI escape processing so the
    // colors work in Windows Terminal / modern conhost.
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    Options opt;
    int exitCode = 0;
    if (!parseArgs(argc, argv, opt, exitCode)) return exitCode;

    std::error_code ec;
    if (!fs::exists(opt.root, ec) || !fs::is_directory(opt.root, ec)) {
        std::cerr << AppName << ": '" << opt.root << "' is not a directory\n";
        return 1;
    }

    // Print the root, colored like a directory.
    if (opt.color) {
        std::cout << COL_DIR << opt.root << COL_OFF << '\n';
    }
    else {
        std::cout << opt.root << '\n';
    }

    Counts counts;
    walk(opt.root, "", 1, opt, counts);

    std::cout << '\n'
              << counts.dirs  << (counts.dirs  == 1 ? " directory, " : " directories, ")
              << counts.files << (counts.files == 1 ? " file"        : " files")
              << '\n';
    return 0;
}
