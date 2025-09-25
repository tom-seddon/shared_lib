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
    const char *a = str.data(), *end = a + str.size(), *b = a;
    while (b != end) {
        char c = *b;
        if (c == '\r' || c == '\n') {
            if (!fun(std::string_view(a, (size_t)(b - a)))) {
                return false;
            }

            ++b;
            if ((*b == '\r' || *b == '\n') && *b != c) {
                ++b;
            }

            a = b;
        } else {
            ++b;
        }
    }

    return true;
}
