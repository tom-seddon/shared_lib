#include <shared/system.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdlib.h>
#include <execinfo.h>
#include <uuid/uuid.h>
#include <shared/debug.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <sys/prctl.h>
#include <map>
#include <algorithm>
#include <shared/strings.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* If true, use addr2line to get symbols from backtrace_symbols. (The
 * standard libc code doesn't try very hard, and gives poor
 * results.) */
#define USE_ADDR2LINE 1
#define DEBUG_ADDR2LINE 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://stackoverflow.com/questions/3596781/detect-if-gdb-is-running
int IsDebuggerAttached() {
    /* Frankly it all seems like far too much bother. */
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_ADDR2LINE
struct AddressRange {
    uint64_t begin = UINT64_MAX, end = 0;
};
#endif

#if USE_ADDR2LINE
static bool ReadFD(std::vector<char> *output, int fd) {
    char buffer[4096];

    output->clear();

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof buffer);

        if (n < 0) {
            return false;
        } else if (n == 0) {
            break;
        }

        output->insert(output->end(), buffer, buffer + n);
    }

    output->push_back(0);
    return true;
}
#endif

#if USE_ADDR2LINE
static void GetBacktraceSymbolsForModule(std::vector<std::string> *symbols,
                                         void *const *addresses,
                                         int num_addresses,
                                         const std::string &module_name,
                                         const std::vector<AddressRange> &module_address_ranges) {
    std::vector<size_t> indexes;
    int fds[2] = {-1, -1};
    std::vector<char> output;

    ASSERT(num_addresses >= 0);
    indexes.reserve((size_t)num_addresses);

    for (int i = 0; i < num_addresses; ++i) {
        uint64_t address = (uint64_t)(uintptr_t)addresses[i];

        for (size_t j = 0; j < module_address_ranges.size(); ++j) {
            const AddressRange *range = &module_address_ranges[j];
            ASSERT(j == 0 || range->begin >= module_address_ranges[j - 1].begin);
            if (address >= range->begin && address < range->end) {
                indexes.push_back((size_t)i);
                break;
            }
        }
    }

    if (indexes.empty()) {
        return;
    }

    std::vector<std::string> address_strings;
    for (size_t index : indexes) {
        // For good backtraces with ASLR/PIE on Linux, supply an
        // address relative to the base of the module. (See, e.g.,
        // https://github.com/scylladb/seastar/issues/334.)
        //
        // The module address ranges are sorted, so the begin address
        // of the module's first range is (hopefully...) its load
        // address.
        uint64_t offset = (uint64_t)(uintptr_t)addresses[index] - module_address_ranges[0].begin;

        address_strings.push_back(strprintf("0x%" PRIx64, offset));
    }

    std::vector<const char *> argv;
    argv.push_back("/usr/bin/addr2line");
    argv.push_back("-e");
    argv.push_back(module_name.c_str());
    argv.push_back("-fi");
    for (const std::string &address_string : address_strings) {
        argv.push_back(address_string.c_str());
    }

#if DEBUG_ADDR2LINE
    for (const char *arg : argv) {
        printf("%s ", arg);
    }
    printf("\n");
#endif

    argv.push_back(nullptr);

    if (pipe(fds) == -1) {
        goto done;
    }

    {
        int pid = fork();
        if (pid == -1) {
            goto done;
        }

        if (pid == 0) {
            dup2(fds[1], STDOUT_FILENO);
            dup2(fds[1], STDERR_FILENO);

            execv(argv[0], const_cast<char *const *>(&argv[0])); //tsk
            _exit(127);
        } else {
            close(fds[1]);
            fds[1] = -1;

            ReadFD(&output, fds[0]);

            // See
            // https://github.com/tom-seddon/shared_lib/commit/50188f7ad96ff79b67bc65a8034bc735c722284b
            // - should this check for EINTR (etc.) too?
            int status;
            if (waitpid(pid, &status, 0) != pid) {
                goto done;
            }

            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                goto done;
            }

            {
                size_t i = 0;
                char *s = output.data();

                while (i < indexes.size()) {
                    char *loc = strsep(&s, "\n"), *fun = NULL;
                    if (loc != NULL) {
                        fun = strsep(&s, "\n");
                    }

                    if (!loc || !fun) {
                        break;
                    }

                    if (fun[0] != '?' && loc[0] != '?') {
                        std::string *str = &(*symbols)[indexes[i]];
                        if (str->empty()) {
                            *str = strprintf("%p: %s: %s", addresses[indexes[i]], fun, loc);
                        }
                    }

                    ++i;
                }
            }

            /* /\* printf("--------------------------------------------------\n"); *\/ */
            /* /\* printf("%s\n",output); *\/ */
            /* /\* printf("--------------------------------------------------\n"); *\/ */

            /* OuLogDumpBytes(&tmp,output.data(),output.size()); */
        }
    }

done:;
    for (int i = 0; i < 2; ++i) {
        if (fds[i] != -1) {
            close(fds[i]);
            fds[i] = -1;
        }
    }
}
#endif

char **GetBacktraceSymbols(void *const *addresses, int num_addresses) {
#if USE_ADDR2LINE

    if (num_addresses <= 0) {
        /* somebody else's problem... */
        return backtrace_symbols(addresses, num_addresses);
    }

    char *result = NULL;

    std::vector<std::string> symbols;
    symbols.resize((size_t)num_addresses);

    // contents of /proc/PID/maps
    std::vector<char> maps;

    /* I don't know how you're *supposed* to read the /proc files.
     * There's a pretty obvious race condition here. */
    {
        std::string fname = "/proc/" + std::to_string(getpid()) + "/maps";

        int fd = open(fname.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            return nullptr;
        }

        bool good = ReadFD(&maps, fd);

        close(fd);
        fd = -1;

        if (!good) {
            return nullptr;
        }
    }

#if DEBUG_ADDR2LINE
    printf("----------------------------------------------------------------------\n");
    // fingers crossed no overflow.
    printf("%.*s\n", (int)maps.size(), maps.data());
    printf("----------------------------------------------------------------------\n");
#endif

    // Collect modules list by name, and unsorted address ranges for each.
    std::map<std::string, std::vector<AddressRange>> ranges_by_name;
    {
        char *stringp = maps.data();
        for (;;) {
            char *line = strsep(&stringp, "\n");
            if (!line) {
                break;
            }

            size_t line_len = strlen(line);

            std::vector<char> prot, name;
            prot.resize(line_len + 1);
            name.resize(line_len + 1);

            // Module module;
            uint64_t pgoff;

            AddressRange range;
            sscanf(line, "%" PRIx64 "-%" PRIx64 " %s %" PRIx64 " %*x:%*x %*u %s\n",
                   &range.begin,
                   &range.end,
                   prot.data(),
                   &pgoff,
                   name.data());

            if (strlen(name.data()) == 0) {
                continue;
            }

            if (name[0] == '[') {
                continue;
            }

            // repeated string copies :(
            std::vector<AddressRange> *ranges = &ranges_by_name[name.data()];
            ranges->push_back(range);
        }
    }

    // Sort address ranges for each module by begin.
    for (auto &name_and_ranges : ranges_by_name) {
        std::sort(name_and_ranges.second.begin(), name_and_ranges.second.end(),
                  [](const AddressRange &a, const AddressRange &b) {
                      return a.begin < b.begin;
                  });
    }

    // Merge adjacent ranges. (This isn't strictly necessary, but it
    // makes the debug output a bit nicer.)
    for (auto &name_and_ranges : ranges_by_name) {
        if (name_and_ranges.second.size() > 1) {
            auto it = name_and_ranges.second.begin() + 1;
            while (it != name_and_ranges.second.end()) {
                if (it->begin == (it - 1)->end) {
                    (it - 1)->end = it->end;
                    it = name_and_ranges.second.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

#if DEBUG_ADDR2LINE
    for (const auto &name_and_ranges : ranges_by_name) {
        printf("%s:\n", name_and_ranges.first.c_str());
        for (size_t i = 0; i < name_and_ranges.second.size(); ++i) {
            const AddressRange *range = &name_and_ranges.second[i];
            printf("  %zu. 0x%" PRIx64 "-0x%" PRIx64 " (+%" PRIu64 ")\n",
                   i,
                   range->begin,
                   range->end,
                   range->end - range->begin);
        }
    }
#endif

    for (const auto &name_and_ranges : ranges_by_name) {
        GetBacktraceSymbolsForModule(&symbols,
                                     addresses,
                                     num_addresses,
                                     name_and_ranges.first,
                                     name_and_ranges.second);
    }

    /* If any entries are missing, fill them in with the address. */
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (symbols[i].empty()) {
            symbols[i] = strprintf("%p", addresses[i]);
        }
    }

    /* Build output buffer. */
    {
        size_t num_array_bytes = symbols.size() * sizeof(char *);
        size_t num_string_bytes = 0;
        for (const std::string &symbol : symbols) {
            num_string_bytes += symbol.size() + 1;
        }

        result = (char *)malloc(num_array_bytes + num_string_bytes);
        if (result) {
            char **strings = (char **)result;
            char *buffer = result + num_array_bytes, *dest = buffer;

            for (size_t i = 0; i < symbols.size(); ++i) {
                strings[i] = dest;
                size_t n = symbols[i].size();
                memcpy(dest, symbols[i].c_str(), n + 1);
                dest += n + 1;
            }
            ASSERT(dest == result + num_array_bytes + num_string_bytes);
        }
    }

    return (char **)result;

#else

    return backtrace_symbols(addresses, num_addresses);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex32(uint32_t value) {
    return __builtin_ffs((int)value) - 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex32(uint32_t value) {
    if (value == 0) {
        return -1;
    }

    return 31 - __builtin_clz(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex64(uint64_t value) {
    return __builtin_ffsll((int64_t)value) - 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex64(uint64_t value) {
    if (value == 0) {
        return -1;
    }

    return 63 - __builtin_clzll(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits32(uint32_t value) {
    return (size_t)__builtin_popcount(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits64(uint64_t value) {
    return (size_t)__builtin_popcountll(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadNameInternal(const char *name) {
    char tmp[16];
    strlcpy(tmp, name, 16);
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr int64_t NS_PER_SEC = 1000 * 1000 * 1000;
static constexpr double SECS_PER_NS = 1.0 / NS_PER_SEC;

uint64_t GetCurrentTickCount(void) {
    struct timespec r;
    clock_gettime(CLOCK_MONOTONIC_RAW, &r);
    return (uint64_t)(NS_PER_SEC * r.tv_sec + r.tv_nsec);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsFromTicks(uint64_t ticks) {
    return ticks * SECS_PER_NS;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsPerTick(void) {
    return SECS_PER_NS;
}
