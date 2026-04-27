#include <shared/system.h>
#include <shared/path.h>
#include <shared/debug.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <shared/system_specific.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsSeparatorChar(char c) {
    return c == '/' || c == '\\';
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SplitPath(std::string::size_type *last_sep_, std::string::size_type *ext_, const std::string &path) {
    std::string::size_type last_sep = path.find_last_of("\\/");
    if (last_sep_) {
        *last_sep_ = last_sep;
    }

    if (last_sep == std::string::npos) {
        last_sep = 0;
    }

    std::string::size_type ext = path.find_last_of(".");
    if (ext < last_sep) {
        ext = std::string::npos;
    }

    if (ext_) {
        *ext_ = ext;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathWithoutExtension(const std::string &path) {
    std::string::size_type dot;
    SplitPath(nullptr, &dot, path);

    if (dot == std::string::npos) {
        return path;
    } else {
        return path.substr(0, dot);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetFolder(const std::string &path) {
    std::string::size_type last_sep;
    SplitPath(&last_sep, nullptr, path);

    if (last_sep == std::string::npos) {
        return "";
    } else {
        return path.substr(0, last_sep + 1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathWithoutTrailingSeparators(const std::string &path) {
    size_t i = path.size();
    while (i > 0 && PathIsSeparatorChar(path[i - 1])) {
        --i;
    }

    return path.substr(0, i);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathJoined(const std::string &a, const std::string &b) {
    if (a.empty()) {
        return b;
    }

    if (b.empty()) {
        return a;
    }

    if (PathIsAbsolute(b)) {
        return b;
    }

    if (PathIsSeparatorChar(a.back())) {
        return a + b;
    } else {
        return a + DEFAULT_SEPARATOR + b;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathJoined(const std::string &a, const std::string &b, const std::string &c) {
    return PathJoined(PathJoined(a, b), c);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetName(const std::string &path) {
    std::string::size_type last_sep;
    SplitPath(&last_sep, nullptr, path);

    if (last_sep == std::string::npos) {
        return path;
    } else {
        return path.substr(last_sep + 1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetExtension(const std::string &path) {
    std::string::size_type ext;
    SplitPath(nullptr, &ext, path);

    if (ext == std::string::npos) {
        return "";
    } else {
        return path.substr(ext);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int PathCompare(const std::string &a, const std::string &b) {
    size_t ai = 0, bi = 0;

    for (;;) {
        bool ea = ai == a.size();
        bool eb = bi == b.size();

        if (ea && eb) {
            break;
        }

        int ca = a[ai];
        int cb = b[bi];

#if SYSTEM_OSX || SYSTEM_WINDOWS
        // This is very lazy, but this function is never used anywhere where it
        // would be a problem.
        ca = tolower(ca);
        cb = tolower(cb);
#endif

#if SYSTEM_WINDOWS
        if (ca == '\\') {
            ca = '/';
        }

        if (cb == '\\') {
            cb = '/';
        }
#endif

        if (ea || ca < cb) {
            return -1;
        } else if (eb || ca > cb) {
            return 1;
        }

        ++ai;
        ++bi;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsAbsolute(const std::string &path) {
    if (!path.empty()) {
        if (PathIsSeparatorChar(path[0])) {
            return true;
        }

#if SYSTEM_WINDOWS
        if (path.size() >= 2) {
            // Test for drive path - B:\\fred or B:fred or whatever. No need to
            // overthink. If there's any problems, they'll get caught later.
            if (path[1] == ':') {
                return true;
            }
        }
#endif
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsFullySpecified(const std::string &path) {
#if SYSTEM_WINDOWS

    if (path.size() >= 3) {
        // Don't allow B:fred and the like. The actual path then depends on the
        // process's state.
        if (path[1] == ':' && PathIsSeparatorChar(path[2])) {
            return true;
        }

        // Anything starting with \\ is considered fully specified.
        if (path[0] == '\\' && path[1] == '\\') {
            return true;
        }
    }

#else

    if (!path.empty()) {
        if (PathIsSeparatorChar(path[0])) {
            return true;
        }
    }

#endif

    return false;
}
