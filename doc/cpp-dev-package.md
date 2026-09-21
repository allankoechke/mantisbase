@page docs_cpp_dev C++ Dev Package

Use MantisBase as a prebuilt C++ library in your own application — no submodule, no waiting on a full source build. This guide covers downloading the package, wiring it into your build, and running your app.

> Building from source or embedding via submodule instead? See the [Embedding Guide](embedding.md). For the server binary or Docker, see the [Installation Guide](installation.md).

---

## What you get

Each GitHub release ships one C++ dev package per OS (shared library only, no static archives):

| File | Contents |
|---|---|
| `mantisbase_<tag>-linux-cpp-dev.zip` | `libmantisbase.so` for x86-64 (`libs/x86`) and aarch64 (`libs/arm`) + Linux headers |
| `mantisbase_<tag>-windows-cpp-dev.zip` | `libmantisbase.dll` (+ import lib) for x86-64 + Windows headers |

Download from [GitHub Releases](https://github.com/allankoechke/mantisbase/releases) and unzip, e.g. into `mantisbase-linux-cpp-dev/`. Every package contains:

```
CMakeLists.txt   add_subdirectory() entry point (defines the `mantisbase` target)
README.md        quick start
VERSION          release tag
include/         header tree for this OS (mantisbase headers + bundled
                 third-party headers: drogon, trantor, nlohmann/json,
                 jsoncpp, soci, spdlog, fmt, dukglue, duktape, wolfssl, ...)
libs/
  x86/           prebuilt shared library for x86-64
  arm/           prebuilt shared library for aarch64 (Linux only)
lib/cmake/MantisBase/
  MantisBaseConfig.cmake         find_package(MantisBase) entry point
  MantisBaseConfigVersion.cmake  version matching
```

Each `include/` tree is complete for its OS — use only the package matching your platform. Generated headers (`soci-config.h`, wolfSSL `options.h`) differ per OS and are already matched to the shipped binaries.

---

## Prerequisites

**Linux** — the shared library needs its system dependencies at build and run time:

```bash
sudo apt-get update
sudo apt-get install -y libpq-dev uuid-dev
```

(Runtime packages on Debian/Ubuntu are `libpq5` and `libuuid1`.)

**Windows (MinGW)** — no extra packages. The socket/RPC/IP-helper/crypto system libraries (`ws2_32`, `rpcrt4`, `iphlpapi`, `crypt32`) are linked automatically.

**Toolchain** — C++20 compiler (GCC/MinGW 13+) and CMake 3.22+.

---

## Integration

### Option A: `add_subdirectory` (recommended)

```cmake
cmake_minimum_required(VERSION 3.22)
project(my_app)

add_subdirectory(path/to/mantisbase-linux-cpp-dev)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE mantisbase)
```

The right `libs/<arch>/` binary is picked automatically for your CPU.

### Option B: `find_package`

```cmake
# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/mantisbase-linux-cpp-dev
find_package(MantisBase REQUIRED)
target_link_libraries(my_app PRIVATE mantisbase::shared)
```

A version can be requested: `find_package(MantisBase 0.4 REQUIRED)`.

### Option C: manual (without CMake)

Linux — make sure the `.so` is findable at runtime (via `rpath` or `LD_LIBRARY_PATH`):

```bash
g++ -std=c++20 main.cpp -I path/to/include -L path/to/libs/x86 -lmantisbase -Wl,-rpath,path/to/libs/x86 -lpq -luuid -ldl -lpthread -lm -o my_app
```

Windows (MinGW) — keep `libmantisbase.dll` next to your `.exe` at runtime:

```cmake
target_include_directories(my_app PRIVATE path/to/include)
target_link_libraries(my_app PRIVATE path/to/libs/x86/libmantisbase.dll.a ws2_32 rpcrt4 iphlpapi crypt32)
```

### `main.cpp`

```cpp
#include <mantisbase/mantisbase.h>

int main(int argc, char* argv[])
{
    auto app = mb::MantisBase::create(argc, argv);
    return app->run();
}
```

`MantisBase::create()` returns a `std::unique_ptr<MantisBase>` that you own; `run()` starts the HTTP server (blocking). Lifecycle details live in the [Embedding Guide](embedding.md).

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `MantisBase dev package does not support '<system>'` | You unzipped the wrong OS package — download the `-linux-cpp-dev` or `-windows-cpp-dev` zip matching your machine. |
| `MantisBase shared library not found: '.../libs/<arch>/...'` | No binary for your CPU in this package (e.g. 32-bit, or arm on Windows). Check `libs/` for shipped architectures, or [build from source](installation.md). |
| `MantisBase requires libpq / libuuid` (Linux) | Install `libpq-dev` and `uuid-dev` (see Prerequisites). |
| App builds but fails to start: `libmantisbase.so: cannot open shared object file` | The loader can't find the `.so` — set `LD_LIBRARY_PATH` or link with `-Wl,-rpath,<libs dir>`. |
| App builds but fails to start on Windows (`0xc0000135` / missing DLL) | Copy `libmantisbase.dll` next to your `.exe`. |
| `find_package` reports a version mismatch | Request a compatible version (`find_package(MantisBase 0.4 REQUIRED)`); on `0.x` releases minor versions must match. |

---

## Notes

- Dev packages ship **shared libraries only**. If you need static linking (single-binary deployment), [build from source](installation.md) instead.
- The bundled third-party headers (`drogon/`, `trantor/`, `soci/`, ...) are part of the package — don't mix them with system-installed copies.
- `libpq-fe.h` / `uuid.h` are system headers and are intentionally not bundled.
