# MantisBase Developer Library Package

This archive contains prebuilt MantisBase static and shared libraries, public headers, bundled third-party headers, and a CMake package config for embedding MantisBase in your C++ application.

## Layout

```
lib/
  linux/
    static/<architecture>/libmantisbase.a
    shared/<architecture>/libmantisbase.so
  windows/
    static/<architecture>/libmantisbase.a
    shared/<architecture>/libmantisbase.dll (+ import lib libmantisbase.dll.a)
  cmake/MantisBase/
    MantisBaseConfig.cmake          # find_package(MantisBase) entry point
    MantisBaseConfigVersion.cmake   # version matching
include-linux/           # Full header tree for Linux: public mantisbase
                         # headers + bundled 3rd-party headers (argparse,
                         # drogon, trantor, nlohmann/json, jsoncpp, soci,
                         # spdlog, fmt, dukglue, duktape, wolfssl, ...) +
                         # Linux-generated headers
include-windows/         # Same, with Windows-generated headers
VERSION                  # Release tag (e.g. v0.4.0)
```

Architectures included in this release depend on the build matrix (typically `x86-64` and `aarch64` for Linux, `x86-64` for Windows).

Each `include-<os>/` tree is complete for its OS, so add only the one matching
your platform to the header search path. The per-OS split exists because a few
headers are generated from build options and differ per OS (`soci-config.h`,
wolfssl `options.h`).

## System prerequisites (Linux)

The static library does not bundle system dependencies. Install them with:

```bash
sudo apt-get update
sudo apt-get install -y libpq-dev uuid-dev
```

Runtime shared libraries on Debian/Ubuntu are `libpq5` and `libuuid1` (see `docker/Dockerfile` in the source repo).

## Quick integration (CMake, recommended)

Point CMake at the extracted package and use the imported targets. The config
selects the right `lib/<os>/…/<arch>/` binary for your toolchain and appends
the required system libraries automatically (on Windows: `ws2_32`, `rpcrt4`,
`iphlpapi`, `crypt32`):

```cmake
cmake_minimum_required(VERSION 3.22)
project(my_app)

# Tell CMake where the dev package lives:
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/mantisbase-dev
find_package(MantisBase REQUIRED)

add_executable(my_app main.cpp)
# Static is typical for a single-binary deployment. Or mantisbase::shared.
target_link_libraries(my_app PRIVATE mantisbase::static)
```

`main.cpp`:

```cpp
#include <mantisbase/mantisbase.h>

int main(int argc, char* argv[])
{
    auto app = mb::MantisBase::create(argc, argv);
    return app->run();
}
```

`find_package` accepts a version: `find_package(MantisBase 0.4 REQUIRED)`.
Available targets: `mantisbase::static`, `mantisbase::shared`
(plus legacy variables `MantisBase_INCLUDE_DIRS`, `MantisBase_LIBRARIES`).

## Manual integration (without CMake)

Add both include roots and link the matching prebuilt library plus its system
dependencies:

```bash
# Example: Linux x86-64, static
g++ -std=c++20 main.cpp \
  -I path/to/include-linux \
  path/to/lib/linux/static/x86-64/libmantisbase.a \
  -lpq -luuid -ldl -lpthread \
  -o my_app
```

On Windows with MinGW, use `include-windows` and append
`-lws2_32 -lrpcrt4 -liphlpapi -lcrypt32`.

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
- Discord: https://discord.gg/9437XTKRvN
- GitHub Discussions: https://github.com/allankoechke/mantisbase/discussions
- Issues: https://github.com/allankoechke/mantisbase/issues
