# MantisBase Developer Library Package

This archive contains prebuilt MantisBase static and shared libraries, headers, and the generated `config.hpp` for embedding MantisBase in your C++ application.

## Layout

```
lib/
  linux/
    static/<architecture>/libmantisbase.a
    shared/<architecture>/libmantisbase.so
  windows/
    static/<architecture>/libmantisbase.a
    shared/<architecture>/libmantisbase.dll
    shared/<architecture>/libmantisbase.dll.a
include/mantisbase/   # Public headers
VERSION                 # Release tag (e.g. v0.4.0)
```

Architectures included in this release depend on the build matrix (typically `x86-64` and `aarch64` for Linux, `x86-64` for Windows).

## Quick integration (CMake)

Link against the static library for a single-binary deployment, or the shared library if you prefer dynamic linking:

```cmake
# Example: Linux x86-64 static
target_include_directories(your_app PRIVATE path/to/include)
target_link_libraries(your_app PRIVATE
  path/to/lib/linux/static/x86-64/libmantisbase.a
)
```

On Windows with MinGW, also link system libraries used by MantisBase: `ws2_32`, `rpcrt4`, `iphlpapi`, `crypt32`.

See the [Embedding Guide](https://github.com/allankoechke/mantisbase/blob/master/doc/embedding.md) for full integration steps, lifecycle (`MantisBase::create()`), and PostgreSQL runtime notes on Linux.

## Build from source

If you need a different platform, compiler, or feature set, build from source:

```bash
git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git
cd mantisbase
cmake -B build -DMB_BUILD_SHARED_LIB=ON
cmake --build build
```

Linux build dependencies: `libpq-dev`, `uuid-dev`. See [doc/embedding.md](https://github.com/allankoechke/mantisbase/blob/master/doc/embedding.md).

## Help

- Documentation: https://allankoechke.github.io/mantisbase/
- GitHub Discussions: https://github.com/allankoechke/mantisbase/discussions
- Issues: https://github.com/allankoechke/mantisbase/issues
