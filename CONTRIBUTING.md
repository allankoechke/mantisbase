# Contributing to Mantis  
  
Thank you for your interest in contributing to **Mantis**, a lightweight Backend-as-a-Service (BaaS) library built in C++!   
  
## 🚀 Getting Started  
  
### Prerequisites  
  
- **C++20** compatible compiler (GCC >= 8.3.0, Clang >= 7.0.0, or MSVC >= 16.8)  
- **CMake** 3.30 or higher  
- **Git** with submodule support  
  
### Setting Up the Development Environment  
  
#### 1. **Clone the repository with submodules:**  
   ```bash  
   git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git  
   cd mantisbase
   ```

#### 2. Build the project:

```bash
cmake -B build  
cmake --build build
```

#### 3. Run the application:
Check on commandline args by running `./build/mantisbase --help`

```bash
./build/mantisbase serve
```

### 📁 Project Structure
Understanding the codebase organization will help you navigate and contribute effectively.

```
mantis/  
├── include/mantis/         # Public API headers  
├── src/                    # Internal implementation  
├── examples/               # Embedding examples  
├── tests/                  # Unit & integration tests  
├── docker/                 # Docker deployment  
├── 3rdParty/              # Third-party dependencies  
└── CMakeLists.txt         # Build configuration  
```

### 🛠️ Build System
MantisBase uses CMake with the following key dependencies: README.md:46-53

- httplib-cpp: HTTP server framework
- spdlog: Structured logging
- SOCI: Database abstraction layer
- nlohmann/json: JSON processing
- jwt-cpp: JWT token handling
- bcrypt-cpp: Password hashing
- argparse: Command-line parsing
- Build Options
  - *MB_SHARED_DEPS*: Build dependencies as shared libraries (default: ON)
  - *MB_BUILD_TESTS*: Enable test compilation (default: ON)

### 🧪 Testing
Run tests after building:

```bash
cd build  
ctest --build-config Release
```

### 📋 Development Guidelines
#### Code Style
- Follow C++20 standards and best practices
- Use meaningful variable and function names
- Include appropriate comments for complex logic
- Maintain consistent indentation and formatting

#### Commit Messages
Use clear, descriptive commit messages:

```
feat: add JWT authentication middleware  
fix: resolve database connection pooling issue  
docs: update API documentation  
test: add unit tests for table validation  
```

#### Pull Request Process
1. Fork the repository
2. Create a feature branch from master:
```
git checkout -b feature/your-feature-name
```
3. Make your changes with appropriate tests
4. Ensure all tests pass and the build succeeds
5. Submit a pull request with:
   - Clear description of changes
   - Reference to any related issues
   - Screenshots/examples if applicable

### 🎯 Areas for Contribution
Based on the current project status, here are areas where contributions are welcome:

#### High Priority
- Unit + integration tests (⬜ Planned)
- Middleware support (🟡 In Progress)
- Static file serving (⬜ Planned)

#### Medium Priority
- Client/server sync modes (⬜ Planned)
- WebSocket sync support (⬜ Planned)
- Docker-ready deployment (⬜ Planned)

#### Documentation
- API documentation improvements
- Code examples and tutorials
- Architecture documentation

#### 🐛 Reporting Issues
When reporting bugs or requesting features:

1. Search existing issues to avoid duplicates
2. Use issue templates when available
3. Provide detailed information:
    - Operating system and version
    - Compiler and version
    - Steps to reproduce
    - Expected vs actual behavior
    - Relevant logs or error messages

### 🔧 Development Tips
#### Working with Dependencies
All third-party dependencies are managed through git submodules and CMake. When adding new dependencies:

- Add as a git submodule in `3rdParty/`
- Update CMakeLists.txt with appropriate configuration
- Consider creating a separate cmake module for complex integrations

#### Database Development
The system uses SOCI for database abstraction with SQLite as the default backend. When working with database features:

- Test with SQLite first
- Consider multi-database compatibility
- Update migration scripts if needed

#### API Development
MantisBase auto-generates REST APIs from database table definitions. When adding API features:

- Follow RESTful conventions
- Maintain backward compatibility
- Update API documentation

### 📞 Getting Help
GitHub Issues: For bug reports and feature requests
GitHub Discussions: For questions and general discussion
Wiki: For detailed documentation and guides

### 📜 License
By contributing to MantisBase, you agree that your contributions will be licensed under the MIT License.

Thank you for helping make MantisBase better! 🚀