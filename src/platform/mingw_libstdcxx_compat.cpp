// MinGW's shared libstdc++-6.dll does not export two groups of symbols that
// this compiler's headers still reference:
//
//   std::__format::__do_vformat_to<char, 1>   (std::format, C++20)
//   std::__get_once_callable / __get_once_call (std::call_once)
//
// Loading the Python extension then fails with "The specified procedure
// could not be found." Defining them here keeps each module on one shared
// libstdc++. A private copy of libstdc++ in the engine DLL segfaults on
// import, because the extension is linked against the shared one.
//
// This file is compiled into libmantisbase.dll and into the Python
// extension, and only for MinGW GCC. llvm-mingw (Windows ARM) uses libc++.

#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)

#include <format>
#include <mutex>

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L && __GNUC__ >= 13
namespace std {
namespace __format {
template _Sink_iter<char>
__do_vformat_to<char, 1>(_Sink_iter<char>, std::string_view, std::format_context &);
} // namespace __format
} // namespace std
#endif

#ifdef _GLIBCXX_NO_EXTERN_THREAD_LOCAL
namespace std {
void *&__get_once_callable() noexcept {
    static thread_local void *callable = nullptr;
    return callable;
}

std::add_lvalue_reference<void (*)()>::type __get_once_call() noexcept {
    static thread_local void (*call)() = nullptr;
    return call;
}
} // namespace std
#endif

#endif
