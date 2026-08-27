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
#    ifdef _WIN32
#      define MANTISBASE_API __declspec(dllexport)
#    else
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
