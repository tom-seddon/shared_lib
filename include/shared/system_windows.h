#ifdef HEADER_4FEB716E3F5344B6ABD578D92FFA8BAC
/* This is not for inclusion from other headers. */
#error shared_windows.h included twice, probably due to inclusion in a header
#else
#define HEADER_4FEB716E3F5344B6ABD578D92FFA8BAC

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Result may point into static thread-local buffer overwritten on
 * each call.
 */
const char *GetLastErrorDescription(void);

/* Result may point into static thread-local buffer overwritten on
 * each call.
 */
const char *GetErrorDescription(DWORD error);

/* Windows version of backtrace. */
int backtrace(void **array, int size);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetByteString(const wchar_t *str, int code_page);
std::string GetByteString(const std::wstring &str, int code_page);

static inline std::string GetUTF8String(const wchar_t *str) {
    return GetByteString(str, CP_UTF8);
}

static inline std::string GetUTF8String(const std::wstring &str) {
    return GetByteString(str, CP_UTF8);
}

std::wstring GetWideString(const char *str, int code_page = CP_UTF8);
std::wstring GetWideString(const std::string &str, int code_Spage = CP_UTF8);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FARPROC FindProcAddress2(const char *dll, const char *name);

template <class T>
bool FindProcAddress(const char *dll, const char *name, T *proc) {
    if (!*proc) {
        *proc = (T)FindProcAddress2(dll, name);
    }

    return !!*proc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
