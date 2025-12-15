# Code Review: Security, Performance, and Code Quality Analysis

## 📝 Recent Updates

**Entity Name Validation Added (✅):**
- Entity name validation has been implemented in `Entity` and `EntitySchema` constructors
- Validation function `EntitySchema::isValidEntityName()` checks:
  - Name is not empty
  - Name length <= 64 characters  
  - Only alphanumeric and underscore characters allowed
- This addresses the SQL injection risk from table name construction in most cases

**Remaining Work:**
- Entity names from URL path parameters still need validation (file serving routes, schema routes)
- See sections below for specific locations

---

## 🔒 Security Vulnerabilities

### 1. **SQL Injection Risk in Table Name Construction** ✅ PARTIALLY ADDRESSED
**Location:** `src/core/models/entity.cpp` (multiple locations)

**Status:** Entity name validation has been added to Entity and EntitySchema constructors using `EntitySchema::isValidEntityName()`, which validates:
- Name is not empty
- Name length <= 64 characters
- Only alphanumeric and underscore characters allowed

**Remaining Concerns:**
- Entity names from URL path parameters should also be validated before use
- File serving route (`/api/files/:entity/:file`) extracts entity name from path without validation
- Schema routes may accept entity names from path parameters

**Affected Code:**
```cpp
// router.cpp:426 - File serving route
const auto table_name = req.getPathParamValue("entity"); // Should validate!

// entity_schema_routes_handlers.cpp:10 - Schema lookup
const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id")); // Should validate if it's a name!
```

**Recommendation:**
- Validate entity names from path parameters before using them
- Use `EntitySchema::isValidEntityName()` for all user-provided entity names
- Consider whitelist validation against known entity names from schema cache for additional security

### 2. **SQL Injection in JavaScript Query Interface** (CRITICAL)
**Location:** `src/core/database.cpp:230-325`

**Issue:** The `DatabaseUnit::query()` method accepts raw SQL queries from JavaScript/Duktape without any validation. This allows arbitrary SQL execution.

**Affected Code:**
```cpp
const char *query = duk_require_string(ctx, 0);
// ... later ...
*sql->prepare << query, soci::use(vals), soci::into(data_row);
```

**Recommendation:**
- **Option 1:** Restrict to SELECT queries only (for read-only access)
- **Option 2:** Implement a query whitelist system
- **Option 3:** Remove this feature entirely if not needed
- **Option 4:** Add strict validation and sanitization
- Add query logging for security auditing

### 3. **Path Traversal Vulnerability in File Operations** ✅ PARTIALLY ADDRESSED
**Location:** `src/core/files.cpp`, `src/core/router.cpp`

**Status:** Entity name validation has been added to Entity/EntitySchema constructors. However, entity names from URL path parameters still need validation.

**Current Protection:** 
- ✅ Entity names are validated in constructors using `isValidEntityName()`
- ✅ `sanitizeFilename()` is used for uploaded files
- ⚠️ Entity names from URL path parameters are not validated before use

**Affected Code:**
```cpp
// router.cpp:426 - File serving route
const auto table_name = req.getPathParamValue("entity"); // Should validate before use!
Files::getFilePath(table_name, file_name); // Uses entity_name without validation
```

**Recommendation:**
- Validate entity names from path parameters using `EntitySchema::isValidEntityName()` before passing to file operations
- Add path canonicalization check to ensure final path is within expected directory
- Consider using `fs::canonical()` and verifying it starts with the base directory

**Example Fix:**
```cpp
std::string Files::filePath(const std::string &entity_name, const std::string &filename) {
    // Validate entity name
    if (!isValidEntityName(entity_name)) {
        throw MantisException(400, "Invalid entity name");
    }
    
    auto base_path = fs::path(MantisBase::instance().dataDir()) / "files";
    auto full_path = base_path / entity_name / sanitizeFilename(filename);
    
    // Canonicalize and verify it's within base_path
    auto canonical_path = fs::canonical(full_path);
    auto canonical_base = fs::canonical(base_path);
    
    if (canonical_path.string().find(canonical_base.string()) != 0) {
        throw MantisException(400, "Path traversal detected");
    }
    
    return canonical_path.string();
}
```

### 4. **Weak JWT Secret Key Default** (MEDIUM PRIORITY)
**Location:** `src/mantisbase.cpp:315-319`

**Issue:** Default JWT secret key is hardcoded and weak. While it can be overridden via environment variable, the default should be stronger or require explicit configuration.

**Affected Code:**
```cpp
return getEnvOrDefault("MANTIS_JWT_SECRET", "<our-very-secret-JWT-key>");
```

**Recommendation:**
- Generate a random secret on first run and store it securely
- Require explicit secret configuration in production
- Add warning if default secret is used
- Consider using a key derivation function

### 5. **Password in Error Logs** (LOW PRIORITY - Already Partially Fixed)
**Location:** `src/core/router_httplib_internals.cpp:141-143`

**Issue:** Password is removed from logged body, but the error message could still leak information through timing attacks.

**Current Code:**
```cpp
auto _body = body;
_body.erase("password");
logger::warn("No user found matching given data: \n\t- {}", _body.dump());
```

**Recommendation:**
- Use constant-time comparison for password verification (already using bcrypt, which is good)
- Consider rate limiting login attempts
- Add generic error messages to prevent user enumeration

### 6. **Column Name Injection in queryFromCols** (MEDIUM PRIORITY)
**Location:** `src/core/models/entity.cpp:502-523`

**Issue:** Column names from `columns` vector are concatenated into SQL without validation.

**Affected Code:**
```cpp
where_clause += columns[i] + " = :value";
```

**Recommendation:**
- Validate column names against entity schema
- Use whitelist approach: only allow columns that exist in the entity's field definitions

## ⚡ Performance Improvements

### 1. **String Concatenation in Hot Paths**
**Location:** Multiple locations in `src/core/models/entity.cpp`

**Issue:** String concatenation using `+` operator creates temporary objects.

**Recommendation:**
- Use `std::stringstream` or `std::format` for complex queries
- Consider pre-computing common query templates
- Use string views where possible

**Example:**
```cpp
// Instead of:
std::string query = "SELECT * FROM " + name() + " WHERE " + where_clause;

// Use:
std::string query = std::format("SELECT * FROM {} WHERE {}", name(), where_clause);
```

### 2. **Repeated Database Session Acquisition**
**Location:** Multiple locations

**Issue:** `MantisBase::instance().db().session()` is called repeatedly, which may acquire/release connections from pool.

**Recommendation:**
- Cache session references where possible
- Use RAII patterns for session management
- Consider passing session as parameter to avoid repeated lookups

### 3. **Inefficient JSON Parsing**
**Location:** `src/core/database.cpp:266-275`

**Issue:** JSON is parsed from string, then immediately iterated. Could be optimized.

**Recommendation:**
- Consider direct Duktape to SOCI value conversion
- Cache parsed JSON objects if reused
- Use JSON streaming parser for large objects

### 4. **Schema Cache Lookups**
**Location:** `src/core/router.cpp` and entity operations

**Issue:** Schema lookups may involve multiple map lookups and JSON parsing.

**Recommendation:**
- Cache frequently accessed schemas
- Use `std::unordered_map` with better hash functions
- Consider pre-loading all schemas at startup

### 5. **File Path Construction**
**Location:** `src/core/files.cpp`

**Issue:** Paths are constructed as strings, then converted to `fs::path`, then back to string.

**Recommendation:**
- Work with `fs::path` objects directly
- Cache base paths
- Use `fs::path` concatenation instead of string concatenation

## 🛠️ General Improvements

### 1. **Error Handling Consistency**
**Issue:** Mix of exceptions, return codes, and optional types for error handling.

**Recommendation:**
- Standardize on exception-based error handling for exceptional cases
- Use `std::optional` for "not found" scenarios
- Use `std::expected` (C++23) or similar for operations that can fail

### 2. **Magic Numbers and Strings**
**Location:** Throughout codebase

**Issue:** Hardcoded values like `"auth"`, `"base"`, `"view"`, status codes, etc.

**Recommendation:**
- Define constants/enums for entity types
- Use named constants for HTTP status codes
- Create configuration constants file

**Example:**
```cpp
namespace EntityType {
    constexpr const char* BASE = "base";
    constexpr const char* AUTH = "auth";
    constexpr const char* VIEW = "view";
}
```

### 3. **Code Duplication**
**Location:** Multiple locations

**Issue:** Similar patterns repeated across files (e.g., password removal, error responses).

**Recommendation:**
- Extract common patterns into utility functions
- Create response builder helpers
- Use templates for similar operations

### 4. **Logging Levels**
**Issue:** Some critical operations use `trace` level, some use `warn` inconsistently.

**Recommendation:**
- Standardize logging levels:
  - `trace`: Detailed debugging
  - `debug`: General debugging
  - `info`: Important operations
  - `warn`: Recoverable issues
  - `error`: Errors that need attention
  - `critical`: System-threatening issues

### 5. **Type Safety**
**Issue:** Heavy use of `std::string` for entity types, field types, etc.

**Recommendation:**
- Use strong types/enums where possible
- Consider `std::string_view` for read-only string parameters
- Use type aliases for common types

## 🎯 Simplification Opportunities

### 1. **Simplify Entity Query Building**
**Location:** `src/core/models/entity.cpp`

**Issue:** Complex string building for SQL queries.

**Recommendation:**
- Create a query builder class
- Use template-based query construction
- Consider using an ORM-like abstraction

### 2. **Reduce Nesting in Middleware**
**Location:** `src/core/middlewares.cpp`

**Issue:** Deeply nested conditionals in middleware functions.

**Recommendation:**
- Use early returns
- Extract helper functions
- Use guard clauses

### 3. **Simplify Configuration Management**
**Location:** `src/mantisbase.cpp`, `src/parse_cmd.cpp`

**Issue:** Configuration scattered across multiple locations.

**Recommendation:**
- Centralize configuration in a single class
- Use builder pattern for configuration
- Validate configuration at startup

### 4. **Consolidate Database Connection Logic**
**Location:** `src/core/database.cpp`

**Issue:** Database-specific code mixed with general connection logic.

**Recommendation:**
- Use strategy pattern for database backends
- Create database-specific adapter classes
- Reduce conditional compilation

### 5. **Simplify File Operations**
**Location:** `src/core/files.cpp`

**Issue:** Multiple similar functions for file operations.

**Recommendation:**
- Create a unified file manager class
- Use consistent error handling
- Reduce code duplication

## 🔍 Other Issues

### 1. **Memory Management**
**Issue:** Some raw pointers, potential memory leaks in error paths.

**Recommendation:**
- Use smart pointers consistently
- Review all `new`/`delete` usage
- Use RAII patterns

### 2. **Thread Safety**
**Issue:** Some shared state may not be thread-safe.

**Recommendation:**
- Audit all shared state
- Use `std::mutex` or `std::shared_mutex` where needed
- Consider lock-free data structures for read-heavy operations

### 3. **Testing Coverage**
**Issue:** Some critical paths may not be fully tested.

**Recommendation:**
- Add tests for error paths
- Test security edge cases
- Add integration tests for file operations
- Test with malicious inputs

### 4. **Documentation**
**Issue:** Some complex functions lack documentation.

**Recommendation:**
- Add Doxygen comments for all public APIs
- Document security considerations
- Add examples for complex operations

### 5. **Dependency Management**
**Issue:** Some dependencies may have security vulnerabilities.

**Recommendation:**
- Regularly audit dependencies
- Keep dependencies updated
- Consider using dependency scanning tools

## 📋 Priority Summary

### Critical (Fix Immediately)
1. SQL injection in JavaScript query interface
2. ✅ **RESOLVED:** Table name validation in SQL queries - Entity name validation added to constructors
3. **REMAINING:** Validate entity names from URL path parameters (file serving, schema routes)

### High Priority (Fix Soon)
1. Path traversal in file operations
2. Column name validation in queryFromCols
3. JWT secret key management

### Medium Priority (Plan for Next Release)
1. Performance optimizations
2. Error handling standardization
3. Code simplification

### Low Priority (Technical Debt)
1. Code duplication
2. Documentation improvements
3. Testing coverage

## 🎬 Recommended Action Plan

1. **Immediate:** Fix SQL injection vulnerabilities
2. **Week 1:** Add input validation for all user-controlled data
3. **Week 2:** Implement path traversal protection
4. **Week 3:** Performance optimizations
5. **Ongoing:** Code quality improvements and refactoring

