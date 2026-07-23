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
