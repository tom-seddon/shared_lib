#include <shared/system.h>
#include <shared/strings.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintf(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string result = strprintfv(fmt, v);
    va_end(v);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt, va_list v) {
    char *str;
    if (vasprintf(&str, fmt, v) == -1) {
        // Better suggestions welcome... please.
        return std::string("vasprintf failed - ") + strerror(errno) + " (" + std::to_string(errno) + ")";
    } else {
        std::string result(str);

        free(str);
        str = NULL;

        return result;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ForEachLine(const std::string &str, std::function<bool(const std::string_view &line)> fun) {
    std::string::size_type a = 0, b = a, n = str.size();
    const char *data = str.data();

    while (b != n) {
        char c = str[b];
        if (c == '\r' || c == '\n') {
            if (!fun(std::string_view(data + a, b - a))) {
                return false;
            }

            ++b;
            if (b < n) {
                char c2 = str[b];
                if ((c2 == '\r' || c2 == '\n') && c2 != c) {
                    ++b;
                }
            }

            a = b;
        } else {
            ++b;
        }
    }

    if (b != a) {
        if (!fun(std::string_view(data + a, b - a))) {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
