# Mantis Admin Dashboard
Release brach for the `master` branch targetted at C++ integration. This branch should be updated automatically with every release of the `master` branch.

## What's the plan here?
- When master is tagged for release, build `master` artefacts
- Add the artefacts to this branch 
- C++ project pulls this branch for integration directly.

## Library Usage

The `mantisbase_admin` library provides embedded web assets (HTML, CSS, JS, images) compiled into your C++ application using CMakeRC.

### Linking

In your CMakeLists.txt:

```cmake
target_link_libraries(your_app PRIVATE mantisbase_admin)
```

### Accessing Resources

In your C++ code:

```cpp
#include <cmrc/cmrc.hpp>

// Declare the resource namespace
CMRC_DECLARE(mantis);

int main() {
    // Get filesystem handle
    auto fs = cmrc::mantis::get_filesystem();
    
    // Open a resource file
    auto resource = fs.open("index.html");
    std::string content(resource.begin(), resource.end());
    
    // Check if file exists
    if (fs.is_file("assets/style.css")) {
        auto file = fs.open("assets/style.css");
    }
    
    // Iterate directory
    for (auto& entry : fs.iterate_directory("public")) {
        if (entry.is_file()) {
            std::cout << entry.filename() << std::endl;
        }
    }
}
```

### CMRC API Reference

- `cmrc::<namespace>::get_filesystem()` - Get handle to embedded filesystem
- `open(path)` - Open a file, returns `cmrc::file`
- `is_file(path)` - Check if path is a file
- `is_directory(path)` - Check if path is a directory
- `exists(path)` - Check if path exists
- `iterate_directory(path)` - Iterate directory contents

See [CMRC Documentation](https://github.com/vector-of-bool/cmrc) for more details.
