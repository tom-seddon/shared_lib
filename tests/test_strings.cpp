#include <shared/system.h>
#include <shared/strings.h>
#include <vector>
#include <shared/testing.h>

static std::vector<std::string> GetLines(const std::string &str, size_t max_size = UINT64_MAX) {
    TEST_GT_UU(max_size, 0);
    std::vector<std::string> lines;
    ForEachLine(str, [&lines, max_size](const std::string_view &line) -> bool {
        lines.push_back(std::string(line));
        if (lines.size() == max_size) {
            return false;
        }

        return true;
    });
    return lines;
}

int main() {
    std::vector<std::string> lines;

    for (bool trailing : {true, false}) {
        for (const char *newline : {"\n", "\r", "\r\n", "\n\r"}) {
            printf("Newline: %d,%d; trailing: %d\n", newline[0], newline[1], trailing);
            std::string str = std::string("a") + newline + "b" + newline + "c";
            if (trailing) {
                str += newline;
            }

            lines = GetLines(str);
            TEST_EQ_UU(lines.size(), 3);
            TEST_EQ_SS(lines[0], "a");
            TEST_EQ_SS(lines[1], "b");
            TEST_EQ_SS(lines[2], "c");
        }
    }
}
