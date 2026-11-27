/**
 * @file export.h
 * @brief DLL/shared-library symbol visibility macros for MantisBase.
 *
 * Defines @ref MANTISBASE_API for marking public symbols when building or consuming
 * MantisBase as a shared library. Empty when `MANTISBASE_STATIC` is defined.
 */

#ifndef MANTISBASE_EXPORT_H
#define MANTISBASE_EXPORT_H

#ifdef MANTISBASE_STATIC
#  define MANTISBASE_API
#else
#  ifdef MantisBase_EXPORTS
#    if defined(_WIN32) && !defined(__GNUC__) && !defined(__clang__)
#      define MANTISBASE_API __declspec(dllexport)
#    else
       // MinGW (GCC and llvm-mingw clang) must not mark symbols dllexport.
       // One dllexport turns off "export every global". GNU ld can be forced
       // back with --export-all-symbols; lld still drops the drogon and soci
       // vtables the Python extension imports, and the first use segfaults.
       // visibility("default") keeps the public API exported, and with no
       // dllexport the linker auto-exports the rest.
#      define MANTISBASE_API __attribute__((visibility("default")))
#    endif
#  else
#    ifdef _WIN32
#      define MANTISBASE_API __declspec(dllimport)
#    else
#      define MANTISBASE_API
#    endif
#  endif
#endif

#endif // MANTISBASE_EXPORT_H
