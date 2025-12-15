# Test Infrastructure Analysis

## 🔒 Security Issues

### 1. **Hardcoded Passwords in Tests** (MEDIUM)
**Location:** All integration tests

**Issue:** Test passwords are hardcoded in source code:
- `adminpass123`
- `testpass123`
- `userpass123`

**Risk:** If test code is committed to public repositories, these passwords could be discovered.

**Recommendation:**
- Use environment variables or test configuration for passwords
- Generate random passwords per test run
- Use a test password manager

**Example Fix:**
```cpp
// In test_fixure.h or test config
inline std::string getTestPassword() {
    static std::string pwd = mb::getEnvOrDefault("TEST_PASSWORD", 
        mb::generateShortId(16)); // Random default
    return pwd;
}
```

### 2. **Temporary Directory Permissions** (LOW)
**Location:** `test_fixure.h:13-31`, `test_jwt.cpp:13,30,49,72`

**Issue:** Test directories created in `/tmp` may be accessible to other users on multi-user systems.

**Current Code:**
```cpp
auto base_path = fs::path("/tmp") / "mantisbase_tests" / mb::generateShortId();
```

**Recommendation:**
- Set restrictive permissions on test directories (700 or 750)
- Use `fs::temp_directory_path()` consistently (already done in some places)
- Consider using `mkdtemp` or similar secure temp directory creation

**Example Fix:**
```cpp
inline fs::path getBaseDir() {
    auto base_path = fs::temp_directory_path() / "mantisbase_tests" / mb::generateShortId();
    fs::create_directories(base_path);
    // Set restrictive permissions (owner read/write/execute only)
    fs::permissions(base_path, fs::perms::owner_all, fs::perm_options::replace);
    return base_path;
}
```

### 3. **Incomplete Cleanup on Test Failure** (LOW)
**Location:** `main.cpp:47`, `test_fixure.h:71-90`

**Issue:** If tests crash or are interrupted, temporary directories may not be cleaned up.

**Recommendation:**
- Use RAII pattern for cleanup
- Register cleanup handlers (atexit, signal handlers)
- Add cleanup verification in CI/CD

### 4. **Static TestFixture Singleton** (LOW)
**Location:** `test_fixure.h:47-50`

**Issue:** Static singleton prevents parallel test execution and can cause test interference.

**Current Code:**
```cpp
static TestFixture& instance(const mb::json& config) {
    static TestFixture _instance{config};
    return _instance;
}
```

**Recommendation:**
- Use thread-local storage or per-test-instance fixtures
- Consider using Google Test's `SetUpTestSuite` / `TearDownTestSuite` for shared setup

### 5. **Hardcoded Port Numbers** (LOW)
**Location:** `test_fixure.h:36`, `test_jwt.cpp:14,31,50,73`

**Issue:** Hardcoded ports (7075, 7076, 7077, etc.) can conflict with running instances.

**Recommendation:**
- Use dynamic port allocation (bind to port 0, then query assigned port)
- Use environment variable for test port
- Check port availability before binding

---

## ⚡ Efficiency Issues

### 1. **Fixed Sleep Delays** (HIGH)
**Location:** `main.cpp:41`, integration tests

**Issue:** Fixed 2-second sleep in main.cpp is inefficient. Each integration test also waits separately.

**Current Code:**
```cpp
// main.cpp:41
std::this_thread::sleep_for(std::chrono::seconds(2));
```

**Recommendation:**
- Use `waitForServer()` method instead of fixed sleep
- Implement exponential backoff with maximum timeout
- Cache server ready state

**Example Fix:**
```cpp
// In main.cpp, replace fixed sleep with:
if (!TestFixture::instance(args).waitForServer(20, 100)) {
    std::cerr << "Server failed to start within timeout\n";
    return 1;
}
```

### 2. **Redundant Server Wait Logic** (MEDIUM)
**Location:** All integration test `SetUp()` methods

**Issue:** Each test class waits for server separately, even though server starts once in `main.cpp`.

**Current Pattern:**
```cpp
void SetUp() override {
    // Wait for server (redundant - server already started in main)
    int retries = 50;
    for (int i = 0; i < retries; ++i) {
        if (auto res = client->Get("/api/v1/health"); res && res->status == 200) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

**Recommendation:**
- Remove redundant waits from individual test SetUp methods
- Use `TestFixture::waitForServer()` if needed
- Add server ready flag to TestFixture

### 3. **No Test Parallelization** (MEDIUM)
**Location:** Test execution

**Issue:** All tests run sequentially, even unit tests that don't need server.

**Recommendation:**
- Enable Google Test parallel execution: `--gtest_parallel=1`
- Separate unit tests (no server) from integration tests (need server)
- Use test sharding for CI/CD

### 4. **Inefficient Test Database Creation** (LOW)
**Location:** `test_jwt.cpp`

**Issue:** Each JWT test creates a new MantisBase instance and database.

**Recommendation:**
- Use shared test fixture for JWT tests
- Reuse database connections where possible

---

## 🔄 Recent Security Changes Impact

### 1. **Rate Limiting on Login Endpoint** (HIGH)
**Location:** Integration auth tests

**Issue:** Login endpoint is rate-limited to 5 requests per minute. Tests making multiple login attempts may hit rate limits.

**Current Risk:**
- `test_integration_auth.cpp` makes multiple login calls
- If tests run slowly or are retried, rate limits may be hit

**Recommendation:**
- Add rate limit bypass for tests (test mode flag)
- Space out login attempts in tests
- Use different IP addresses for different test cases (if testing rate limiting)
- Add test-specific rate limit configuration

**Example Fix:**
```cpp
// In test setup, configure higher rate limits for tests
if (getenv("TEST_MODE")) {
    // Configure test rate limits (higher than production)
    // Or disable rate limiting in test mode
}
```

### 2. **Entity Name Validation** (ADDRESSED)
**Status:** ✅ Covered in `test_files_security.cpp`

### 3. **Path Traversal Protection** (ADDRESSED)
**Status:** ✅ Covered in `test_files_security.cpp`

### 4. **IP Validation** (MISSING)
**Issue:** No tests for IP validation utilities (`isValidIPv4`, `isValidIPv6`, `isValidIP`)

**Recommendation:**
- Add unit tests for IP validation functions
- Test edge cases (invalid formats, boundary values)

---

## 🏗️ Test Restructuring Recommendations

### 1. **Separate Unit and Integration Tests**

**Current Structure:**
```
tests/
├── unit/
├── integration/
├── main.cpp (starts server for all tests)
└── test_fixure.h
```

**Recommended Structure:**
```
tests/
├── unit/
│   ├── test_*.cpp (no server needed)
│   └── CMakeLists.txt (separate executable)
├── integration/
│   ├── test_*.cpp (needs server)
│   └── CMakeLists.txt (separate executable with server)
├── common/
│   └── test_helpers.h (shared utilities)
└── fixtures/
    └── test_fixure.h
```

**Benefits:**
- Unit tests run faster (no server startup)
- Can parallelize unit tests independently
- Clearer separation of concerns

### 2. **Improve Test Fixture Design**

**Current Issues:**
- Static singleton prevents parallelization
- No per-test isolation
- Shared state between tests

**Recommended Approach:**
```cpp
// Use Google Test's SetUpTestSuite for shared setup
class IntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Start server once for all tests in suite
        if (!server_started) {
            startTestServer();
            server_started = true;
        }
    }
    
    void SetUp() override {
        // Per-test setup (clean state)
        client = std::make_unique<httplib::Client>("http://localhost", test_port);
        // Wait for server if needed
    }
    
    void TearDown() override {
        // Per-test cleanup
        cleanupTestData();
    }
    
private:
    static bool server_started;
    static int test_port;
};
```

### 3. **Add Test Configuration**

**Create `test_config.h`:**
```cpp
namespace TestConfig {
    inline std::string getTestPassword() {
        return mb::getEnvOrDefault("TEST_PASSWORD", 
            "test_" + mb::generateShortId(12));
    }
    
    inline int getTestPort() {
        static int port = []() {
            auto env_port = mb::getEnvOrDefault("TEST_PORT", "");
            return env_port.empty() ? 0 : std::stoi(env_port); // 0 = auto
        }();
        return port;
    }
    
    inline bool isRateLimitingEnabled() {
        return mb::getEnvOrDefault("TEST_DISABLE_RATE_LIMIT", "0") == "0";
    }
}
```

### 4. **Add Test Utilities**

**Create `tests/common/test_helpers.h`:**
```cpp
namespace TestHelpers {
    // Wait for server with better error handling
    bool waitForServer(httplib::Client& client, int max_retries = 20, int delay_ms = 100);
    
    // Create test admin token
    std::string createTestAdminToken(httplib::Client& client);
    
    // Cleanup test entity
    void cleanupTestEntity(httplib::Client& client, const std::string& entity_name, 
                          const std::string& token);
    
    // Generate test data
    nlohmann::json generateTestUser(const std::string& email_prefix = "test");
}
```

---

## 📋 Priority Action Items

### Immediate (Security)
1. ✅ Add IP validation unit tests
2. ⚠️ Address rate limiting in integration tests
3. ⚠️ Fix hardcoded passwords (use env vars or random generation)
4. ⚠️ Set restrictive permissions on test directories

### Short-term (Efficiency)
1. ⚠️ Replace fixed sleep with `waitForServer()` in main.cpp
2. ⚠️ Remove redundant server waits from integration test SetUp methods
3. ⚠️ Enable test parallelization for unit tests

### Medium-term (Structure)
1. ⚠️ Separate unit and integration test executables
2. ⚠️ Refactor TestFixture to support parallelization
3. ⚠️ Add test configuration system
4. ⚠️ Create test helper utilities

---

## 🎯 Recommended Implementation Order

1. **Phase 1: Security Fixes**
   - Add IP validation tests
   - Fix rate limiting in tests
   - Secure test directory permissions

2. **Phase 2: Efficiency Improvements**
   - Fix sleep delays
   - Remove redundant waits
   - Enable parallelization

3. **Phase 3: Restructuring**
   - Separate test executables
   - Refactor fixtures
   - Add test utilities

