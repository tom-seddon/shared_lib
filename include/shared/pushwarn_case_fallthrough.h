#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#elif defined _MSC_VER
#pragma warning(push)
// C5262: implicit fall-through occurs here; are you missing a break statement?
#pragma warning(disable : 5262)
#endif
