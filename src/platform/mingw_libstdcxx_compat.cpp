// MinGW's shared libstdc++-6.dll does not export symbols that GCC 16's
// headers still reference:
//
//   std::__format::__do_vformat_to<char, 1>   (std::format, C++20)
//   basic_format_arg::_M_handle_unrecognized  (called by that instantiation)
//   std::__get_once_callable / __get_once_call (std::call_once)
//
// Loading the Python extension then fails with "The specified procedure
// could not be found." Defining them here keeps each module on one shared
// libstdc++. A private copy of libstdc++ in the engine DLL segfaults on
// import, because the extension is linked against the shared one.
//
// This file is compiled into libmantisbase.dll and into the Python
// extension, and only for MinGW GCC. llvm-mingw (Windows ARM) uses libc++.
//
// Do not turn _GLIBCXX_EXTERN_TEMPLATE off to emit the format body. That
// also emits local copies of string, locale, and exception typeinfo, and
// those copies disagree with the shared libstdc++ the other module uses.
// The explicit instantiation below emits only __do_vformat_to. The
// unrecognized-argument function stays an import (its body is not in the
// header while extern templates are on), so it is supplied by name.
//
// std::call_once writes the callable through __get_once_* and then
// pthread_once invokes __once_proxy. libstdc++'s proxy reads its own
// thread-local, which is still null, so the first call_once calls a null
// function pointer. The proxy has to be the one in this module, reading
// these getters.

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

// Exact symbol the instantiation calls. The asm name avoids the extra
// underscore MinGW adds to extern "C" on 32-bit. It never returns.
void mb_format_handle_unrecognized() asm(
    "_ZNKSt16basic_format_argISt20basic_format_contextINSt8__format10_Sink_iterIcEEcEE22_M_handle_unrecognizedEv");

void mb_format_handle_unrecognized() {
    throw std::format_error("format error: unrecognized argument type");
}
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

extern "C" void __once_proxy(void) {
    auto &slot = std::__get_once_call();
    void (*func)() = slot;
    slot = nullptr;
    if (func)
        func();
}
#endif

#endif
