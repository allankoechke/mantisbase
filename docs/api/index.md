---
generator: doxide
---


# MantisBase Documentation

User and developer docs for navigating the use and integration of MantisBase, an open source, single-binary, lightweight BaaS toolkit written in C++.

## Types

| Name | Description |
| ---- | ----------- |
| [KeyValStore](KeyValStore.md) | Manages application settings  |
| [MantisBase](MantisBase.md) | MantisBase entry point. |
| [Database](Database.md) | Database connection and session management. |
| [MantisRequest](MantisRequest.md) | A wrapper class around `httplib::Request` offering a consistent API and allowing for easy wrapper methods compatible with `Duktape` API requirements for scripting. |
| [MantisResponse](MantisResponse.md) | Wrapper around httplib::Response for consistent API. Provides convenient methods for setting response status, headers, and body with support for JSON, text, HTML, and file responses. ```cpp // Send JSON response res.sendJSON(200, {{"message", "Success"}}); // Send text response res.sendText(200, "Hello World"); // Set custom header res.setHeader("X-Custom", "value"); ```  |
| [MantisContentReader](MantisContentReader.md) | Middleware shorthand for the content reader  |
| [Auth](Auth.md) | JWT token creation and verification utilities. |
| [Files](Files.md) | File management for entity file assets. |
| [MantisException](MantisException.md) | Custom exception class for MantisBase errors. |
| [ContextStore](ContextStore.md) | The `ContextStore` class provides a means to set/get a key-value data that can be shared uniquely between middlewares and the handler functions. |

## Type Aliases

| Name | Description |
| ---- | ----------- |
| [json](#json) | JSON convenience for the nlomann::json namespace  |

## Functions

| Name | Description |
| ---- | ----------- |
| [joinPaths](#joinPaths) | Joins absolute path and a relative path, relative to the first path. |
| [resolvePath](#resolvePath) | Resolves given path as a string to an absolute path. |
| [createDirs](#createDirs) | Create directory, given a path Creates the target directory iteratively, including any missing parent directories. |
| [dirFromPath](#dirFromPath) | Returns a created/existing directory from a path. |
| [toLowerCase](#toLowerCase) | Converts a string to its lowercase variant. |
| [toUpperCase](#toUpperCase) | Converts a string to its uppercase variant. |
| [trim](#trim) | Trims leading and trailing whitespaces from a string. |
| [tryParseJsonStr](#tryParseJsonStr) | Attempt to parse a JSON string. |
| [strToBool](#strToBool) | Convert given string value to boolean type. |
| [generateTimeBasedId](#generateTimeBasedId) | Generate a time base UUID The first part is made up of milliseconds since epoch while the last 4 digits a random component. |
| [generateReadableTimeId](#generateReadableTimeId) | Generates a readable time-based UUID. |
| [generateShortId](#generateShortId) | Generates a short UUID Similar to what platforms like YouTube use for videos, but in our case, making use of only alphanumeric characters. |
| [splitString](#splitString) | Split given string based on given delimiter :material-location-enter: `input` :    Input string to split. :material-location-enter: `delimiter` :    The string delimiter to use to split the `input` string. :material-keyboard-return: **Return** :    A vector of strings. ```cpp auto parts = splitString("Hello, John!", ","); std::cout << parts.size() << std::endl; // > Should be a vector of two strings `Hello` and ` World` ```  |
| [getEnvOrDefault](#getEnvOrDefault) | Retrieves a value from an environment variable or a default value if the env variable was not set. |
| [invalidChar](#invalidChar) | Check if a character is invalid in a filename. |
| [sanitizeInPlace](#sanitizeInPlace) | Sanitize a string in-place by removing or replacing invalid characters. |
| [sanitizeFilename](#sanitizeFilename) | Sanitize a filename and ensure uniqueness. |
| [hashPassword](#hashPassword) | Digests user password + a generated salt to yield a hashed password. |
| [verifyPassword](#verifyPassword) | Verifies user password if it matches the given hashed password. |
| [tmToStr](#tmToStr) | Convert c++ std::tm date/time value to ISO formatted string. :material-location-enter: `t` :    std::tm value :material-keyboard-return: **Return** :    ISO formatted datetime value  |
| [strToTM](#strToTM) | Convert ISO formatted datetime string to std::tm structure. :material-location-enter: `value` :    ISO formatted datetime string :material-keyboard-return: **Return** :    std::tm structure representing the datetime  |
| [dbDateToString](#dbDateToString) | Convert database date value from SOCI row to string. |
| [safe_stoi](#safe_stoi) | Safely convert string to integer with default fallback. |
| [isValidIPv4](#isValidIPv4) | Validates if a string is a valid IPv4 address. |
| [isValidIPv6](#isValidIPv6) | Validates if a string is a valid IPv6 address. |
| [isValidIP](#isValidIP) | Validates if a string is a valid IP address (IPv4 or IPv6). |
| [json2SociValue](#json2SociValue) | Convert fields data to a corresponding soci::values object with a ref to the entity schema the fields belong to. :material-location-enter: `entity` :    Reference entity object for schema validation. :material-location-enter: `fields` :    JSON Array of fields to be converted to SOCI types. :material-keyboard-return: **Return** :    An instance of soci::values object which matches the input fields.  |
| [sociRow2Json](#sociRow2Json) | Convert database response of type soci::row to a corresponding JSON representation. :material-location-enter: `row` :    Reference row record from the database query. :material-location-enter: `entity_fields` :    Reference field schema for converting to/from soci types. :material-keyboard-return: **Return** :    JSON Object representation of the soci::row data.  |
| [generate_uuidv7](#generate_uuidv7) | Generator for uuid v7 based on the spec. |
| [getAuthToken](#getAuthToken) | Extract and validate JWT token from Authorization header. |
| [hydrateContextData](#hydrateContextData) | Hydrate request context with additional data. |
| [hasAccess](#hasAccess) | Check if request has access to entity based on access rules. |
| [requireExprEval](#requireExprEval) | Require expression evaluation to pass. |
| [requireGuestOnly](#requireGuestOnly) | Require guest-only access (no authentication). |
| [requireAdminAuth](#requireAdminAuth) | Require admin authentication. |
| [requireAdminOrEntityAuth](#requireAdminOrEntityAuth) | Require admin OR entity authentication. |
| [requireEntityAuth](#requireEntityAuth) | Require entity-specific authentication. |
| [rateLimit](#rateLimit) | Rate limiting middleware to prevent abuse. |

## Type Alias Details

### json<a name="json"></a>

!!! typedef "using json = nlohmann::json"

    JSON convenience for the nlomann::json namespace
    

## Function Details

### createDirs<a name="createDirs"></a>
!!! function "bool createDirs(const fs::path &amp;path)"

    Create directory, given a path
    
    Creates the target directory iteratively, including any missing parent directories.
    his ensures, any parent directory is set up before attempting to create a child directory.
    
    
    :material-location-enter: `path`
    :    The directory path to create like `/foo/bar`.
        
    :material-keyboard-return: **Return**
    :    True if creation was successful. If the directory exists, it returns false.
    

### dbDateToString<a name="dbDateToString"></a>
!!! function "std::string dbDateToString(const soci::row &amp;row, int index)"

    Convert database date value from SOCI row to string.
        
    :material-location-enter: `row`
    :    SOCI row containing the date value
        
    :material-location-enter: `index`
    :    Column index in the row
        
    :material-keyboard-return: **Return**
    :    String representation of the date
    

### dirFromPath<a name="dirFromPath"></a>
!!! function "std::string dirFromPath(const std::string &amp;path)"

    Returns a created/existing directory from a path.
    
    Given a file path, it first gets the file parent directory and ensures the directory
    together with any missing parent directories are created first using the `createDirs(...)`.
    
    
    :material-eye-outline: **See**
    :    createDirs() for creating directories.
    
    
    :material-location-enter: `path`
    :    The file path
        
    :material-keyboard-return: **Return**
    :    Returns the directory path if successful, else an empty string.
    

### generateReadableTimeId<a name="generateReadableTimeId"></a>
!!! function "std::string generateReadableTimeId()"

    Generates a readable time-based UUID.
    
    The first segment is the current time in ISO-formatted time + milliseconds + short random suffix.
    It is human-readable and sortable, just like
    
    Sample Output: `20250531T221944517N3J`
    
    
    :material-keyboard-return: **Return**
    :    A readable-time based UUID
    
    
    :material-eye-outline: **See**
    :    generateTimeBasedId() For a time-based UUID.
    
    :material-eye-outline: **See**
    :    generateShortId() For a short UUID.
    

### generateShortId<a name="generateShortId"></a>
!!! function "std::string generateShortId(size_t length = 16)"

    Generates a short UUID
    
    Similar to what platforms like YouTube use for videos, but in our case, making use of only
    alphanumeric characters.
    
    Sample Output: `Fz8xYc6a7LQw`
    
    
    :material-keyboard-return: **Return**
    :    A short alphanumeric UUID
    
    
    :material-eye-outline: **See**
    :    generateTimeBasedId() For a time-based UUID.
    
    :material-eye-outline: **See**
    :    generateReadableTimeId() For a readable time-based UUID.
    

### generateTimeBasedId<a name="generateTimeBasedId"></a>
!!! function "std::string generateTimeBasedId()"

    Generate a time base UUID
    
    The first part is made up of milliseconds since epoch while the last 4 digits a random component.
    This makes it lexicographically sortable by time
    
    Sample Output: `17171692041233276`
    
    
    :material-keyboard-return: **Return**
    :    A time based UUID
    
    
    :material-eye-outline: **See**
    :    generateReadableTimeId() For a readable time-based UUID.
    
    :material-eye-outline: **See**
    :    generateShortId() For a short UUID.
    

### generate_uuidv7<a name="generate_uuidv7"></a>
!!! function "inline std::string generate_uuidv7()"

    Generator for uuid v7 based on the spec.
    
    
    :material-keyboard-return: **Return**
    :    uuidv7 string representation
    

### getAuthToken<a name="getAuthToken"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; getAuthToken()"

    Extract and validate JWT token from Authorization header.
    
    Parses Bearer token from request headers and stores user info in context.
    
    :material-keyboard-return: **Return**
    :    Middleware function
    

### getEnvOrDefault<a name="getEnvOrDefault"></a>
!!! function "std::string getEnvOrDefault(const std::string &amp;key, const std::string &amp;defaultValue)"

    Retrieves a value from an environment variable or a default value if the env variable was not set.
    
    :material-location-enter: `key`
    :    Environment variable key.
        
    :material-location-enter: `defaultValue`
    :    A default value if the key is not set.
        
    :material-keyboard-return: **Return**
    :    The env value if found, else the default value passed in.
    

### hasAccess<a name="hasAccess"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; hasAccess(const std::string&amp; entity_name)"

    Check if request has access to entity based on access rules.
    
    :material-location-enter: `entity_name`
    :    Entity/table name to check access for
        
    :material-keyboard-return: **Return**
    :    Middleware function that validates access rules
        
    ```
        router.Get("/api/v1/posts", handler, {hasAccess("posts")});
        
    ```
    

### hashPassword<a name="hashPassword"></a>
!!! function "std::string hashPassword(const std::string &amp;password)"

    Digests user password + a generated salt to yield a hashed password.
        
    :material-location-enter: `password`
    :    Password input to hash.
        
    :material-keyboard-return: **Return**
    :    A hash string representation of the password + salt.
    

### hydrateContextData<a name="hydrateContextData"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; hydrateContextData()"

    Hydrate request context with additional data.
    
    Populates context store with request metadata and user information.
    
    :material-keyboard-return: **Return**
    :    Middleware function
    

### invalidChar<a name="invalidChar"></a>
!!! function "bool invalidChar(unsigned char c)"

    Check if a character is invalid in a filename.
    
    Invalid characters typically include reserved symbols such as
    slashes, colons, question marks, etc. This function allows
    filtering and sanitization of filenames.
    
    
    :material-location-enter: `c`
    :    Character to check.
        
    :material-keyboard-return: **Return**
    :    true if the character is invalid in a filename, false otherwise.
    

### isValidIP<a name="isValidIP"></a>
!!! function "bool isValidIP(const std::string &amp;ip)"

    Validates if a string is a valid IP address (IPv4 or IPv6).
    
    
    :material-location-enter: `ip`
    :    String to validate
        
    :material-keyboard-return: **Return**
    :    true if valid IPv4 or IPv6, false otherwise
    

### isValidIPv4<a name="isValidIPv4"></a>
!!! function "bool isValidIPv4(const std::string &amp;ip)"

    Validates if a string is a valid IPv4 address.
    
    Checks if the input string matches the IPv4 format (e.g., "192.168.1.1").
    Does not validate the actual IP range, only the format.
    
    
    :material-location-enter: `ip`
    :    String to validate as IPv4 address
        
    :material-keyboard-return: **Return**
    :    true if the string is a valid IPv4 format, false otherwise
    
    ```cpp
    if (isValidIPv4("192.168.1.1")) {
        // Valid IPv4 format
    }
    ```
    

### isValidIPv6<a name="isValidIPv6"></a>
!!! function "bool isValidIPv6(const std::string &amp;ip)"

    Validates if a string is a valid IPv6 address.
    
    Checks if the input string matches the IPv6 format (e.g., "2001:0db8::1").
    Supports compressed notation (::).
    
    
    :material-location-enter: `ip`
    :    String to validate as IPv6 address
        
    :material-keyboard-return: **Return**
    :    true if the string is a valid IPv6 format, false otherwise
    

### joinPaths<a name="joinPaths"></a>
!!! function "fs::path joinPaths(const std::string &amp;path1, const std::string &amp;path2)"

    Joins absolute path and a relative path, relative to the first path.
    
    
    :material-location-enter: `path1`
    :    The first absolute path or relative path
        
    :material-location-enter: `path2`
    :    The relative path, subject to the first
        
    :material-keyboard-return: **Return**
    :    An absolute path if successfully joined, else an empty path.
    

### json2SociValue<a name="json2SociValue"></a>
!!! function "inline soci::values json2SociValue(const json &amp;entity, const json &amp;fields)"

    Convert fields data to a corresponding soci::values object with a ref
    to the entity schema the fields belong to.
    
    
    :material-location-enter: `entity`
    :    Reference entity object for schema validation.
        
    :material-location-enter: `fields`
    :    JSON Array of fields to be converted to SOCI types.
        
    :material-keyboard-return: **Return**
    :    An instance of soci::values object which matches the input fields.
    

### rateLimit<a name="rateLimit"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; rateLimit( int max_requests, int window_seconds, bool use_user_id = false )"

    Rate limiting middleware to prevent abuse.
    
    Limits the number of requests per time window. Can rate limit by IP address
    or by authenticated user ID. When the rate limit is exceeded, returns a
    429 Too Many Requests response with rate limit headers.
    
    
    :material-location-enter: `max_requests`
    :    Maximum number of requests allowed in the time window
        
    :material-location-enter: `window_seconds`
    :    Time window duration in seconds
        
    :material-location-enter: `use_user_id`
    :    If true, rate limit by authenticated user ID; if false, use IP address.
                           Falls back to IP if user ID is not available.
        
    :material-keyboard-return: **Return**
    :    Middleware function that enforces rate limits
    
    
    !!! note
     The login endpoint uses this middleware with 5 requests per 60 seconds by IP.
        
    !!! note
     Rate limit headers are included in responses: X-RateLimit-Limit, X-RateLimit-Remaining,
                  X-RateLimit-Reset, and Retry-After.
    
    
    ```
        // Rate limit by IP: 100 requests per minute
        router.Get("/api/v1/data", handler, {rateLimit(100, 60, false)});
    
    // Rate limit by user: 10 requests per second
    router.Post("/api/v1/upload", handler, {rateLimit(10, 1, true)});
    
    // Login endpoint: 5 attempts per minute per IP (prevents brute force)
    router.Post("/api/v1/auth/login", loginHandler, {rateLimit(5, 60, false)});
    
    ```
    

### requireAdminAuth<a name="requireAdminAuth"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; requireAdminAuth()"

    Require admin authentication.
    
    Only allows requests from users authenticated as admins.
    
    :material-keyboard-return: **Return**
    :    Middleware function
    

### requireAdminOrEntityAuth<a name="requireAdminOrEntityAuth"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; requireAdminOrEntityAuth(const std::string&amp; entity_name)"

    Require admin OR entity authentication.
    
    :material-location-enter: `entity_name`
    :    Entity name for entity-based auth
        
    :material-keyboard-return: **Return**
    :    Middleware function that allows admins or entity users
    

### requireEntityAuth<a name="requireEntityAuth"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; requireEntityAuth(const std::string&amp; entity_name)"

    Require entity-specific authentication.
        
    :material-location-enter: `entity_name`
    :    Entity name to authenticate against
        
    :material-keyboard-return: **Return**
    :    Middleware function that validates entity auth
    

### requireExprEval<a name="requireExprEval"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; requireExprEval(const std::string&amp; expr)"

    Require expression evaluation to pass.
        
    :material-location-enter: `expr`
    :    Expression string to evaluate
        
    :material-keyboard-return: **Return**
    :    Middleware function that evaluates expression
    

### requireGuestOnly<a name="requireGuestOnly"></a>
!!! function "std::function&lt;HandlerResponse(MantisRequest&amp;, MantisResponse&amp;)&gt; requireGuestOnly()"

    Require guest-only access (no authentication).
    
    Blocks authenticated users, only allows unauthenticated requests.
    
    :material-keyboard-return: **Return**
    :    Middleware function
    

### resolvePath<a name="resolvePath"></a>
!!! function "fs::path resolvePath(const std::string &amp;input_path)"

    Resolves given path as a string to an absolute path.
    
    Given a relative path, relative to the `cwd`, we can resolve that path to the actual
    absolute path in our filesystem. This is needed for creating directories and files, especially
    for database and file-serving.
    
    
    :material-location-enter: `input_path`
    :    The path to resolve
        
    :material-keyboard-return: **Return**
    :    Returns an absolute filesystem path.
    

### safe_stoi<a name="safe_stoi"></a>
!!! function "int safe_stoi(const std::string &amp;s, const int default_val)"

    Safely convert string to integer with default fallback.
    
    Attempts to convert a string to an integer. If conversion fails
    (invalid format, out of range, etc.), returns the default value
    instead of throwing an exception.
    
    
    :material-location-enter: `s`
    :    String to convert
        
    :material-location-enter: `default_val`
    :    Default value to return if conversion fails
        
    :material-keyboard-return: **Return**
    :    Converted integer or default value
    
    ```cpp
    int page = safe_stoi(req.getQueryParam("page"), 1);  // Defaults to 1 if invalid
    int limit = safe_stoi(req.getQueryParam("limit"), 100);  // Defaults to 100 if invalid
    ```
    

### sanitizeFilename<a name="sanitizeFilename"></a>
!!! function "std::string sanitizeFilename(std::string_view original, std::size_t maxLen = 50, std::size_t idLen = 12, std::string_view idSep = &quot;_&quot;)"

    Sanitize a filename and ensure uniqueness.
    
    Creates a safe filename from the given input. Invalid characters are
    replaced with underscores. The resulting filename is truncated to
    `maxLen` characters, and a short unique ID is appended to avoid collisions.
    
    Example:
      Input: "my*illegal:file?.txt"
      Output: "my_illegal_file_txt_abC82xM01qP"
    
    
    :material-location-enter: `original`
    :    Original filename (input).
        
    :material-location-enter: `maxLen`
    :    Maximum length of the sanitized name before adding ID. Default = 50.
        
    :material-location-enter: `idLen`
    :    Length of the unique ID suffix. Default = 12.
        
    :material-location-enter: `idSep`
    :    Separator inserted between the name and the ID. Default = "_".
        
    :material-keyboard-return: **Return**
    :    Sanitized filename with appended unique ID.
    

### sanitizeInPlace<a name="sanitizeInPlace"></a>
!!! function "void sanitizeInPlace(std::string &amp;s)"

    Sanitize a string in-place by removing or replacing invalid characters.
    
    This modifies the provided string directly. Characters deemed invalid
    by `invalidChar()` are replaced with an underscore (`_`).
    
    
    :material-location-enter: `s`
    :    Reference to the string to sanitize.
    

### sociRow2Json<a name="sociRow2Json"></a>
!!! function "inline json sociRow2Json(const soci::row &amp;row, const std::vector&lt;json&gt; &amp;entity_fields)"

    Convert database response of type soci::row to a corresponding JSON representation.
    
    
    :material-location-enter: `row`
    :    Reference row record from the database query.
        
    :material-location-enter: `entity_fields`
    :    Reference field schema for converting to/from soci types.
        
    :material-keyboard-return: **Return**
    :    JSON Object representation of the soci::row data.
    

### splitString<a name="splitString"></a>
!!! function "std::vector&lt;std::string&gt; splitString(const std::string &amp;input, const std::string &amp;delimiter)"

    Split given string based on given delimiter
    
    
    :material-location-enter: `input`
    :    Input string to split.
        
    :material-location-enter: `delimiter`
    :    The string delimiter to use to split the `input` string.
        
    :material-keyboard-return: **Return**
    :    A vector of strings.
    
    ```cpp
    auto parts = splitString("Hello, John!", ",");
    std::cout << parts.size() << std::endl;
    // > Should be a vector of two strings `Hello` and ` World`
    ```
    

### strToBool<a name="strToBool"></a>
!!! function "bool strToBool(const std::string &amp;value)"

    Convert given string value to boolean type.
    
    By default, we check for true equivalents, anything else will be
    considered as a false value.
    
    
    :material-location-enter: `value`
    :    String value to convert to bool
        
    :material-keyboard-return: **Return**
    :    true or false value
    

### strToTM<a name="strToTM"></a>
!!! function "std::tm strToTM(const std::string &amp;value)"

    Convert ISO formatted datetime string to std::tm structure.
        
    :material-location-enter: `value`
    :    ISO formatted datetime string
        
    :material-keyboard-return: **Return**
    :    std::tm structure representing the datetime
    

### tmToStr<a name="tmToStr"></a>
!!! function "std::string tmToStr(const std::tm &amp;t)"

    Convert c++ std::tm date/time value to ISO formatted string.
        
    :material-location-enter: `t`
    :    std::tm value
        
    :material-keyboard-return: **Return**
    :    ISO formatted datetime value
    

### toLowerCase<a name="toLowerCase"></a>
!!! function "void toLowerCase(std::string &amp;str)"

    Converts a string to its lowercase variant.
    
    It converts the string in place.
    
    
    :material-location-enter: `str`
    :    The string to convert.
        
    :material-eye-outline: **See**
    :    toUpperCase() To convert string to uppercase.
    

### toUpperCase<a name="toUpperCase"></a>
!!! function "void toUpperCase(std::string &amp;str)"

    Converts a string to its uppercase variant.
    
    It converts the string in place.
    
    
    :material-location-enter: `str`
    :    The string to convert.
        
    :material-eye-outline: **See**
    :    toLowerCase() To convert string to lowercase.
    

### trim<a name="trim"></a>
!!! function "std::string trim(const std::string &amp;s)"

    Trims leading and trailing whitespaces from a string.
    
    
    :material-location-enter: `s`
    :    The string to trim.
        
    :material-keyboard-return: **Return**
    :    String with all leading and trailing whitespaces removed.
    

### tryParseJsonStr<a name="tryParseJsonStr"></a>
!!! function "std::optional&lt;json&gt; tryParseJsonStr(const std::string &amp;json_str, std::optional&lt;json&gt; default_value = std::nullopt)"

    Attempt to parse a JSON string.
        
    :material-location-enter: `json_str`
    :    JSON string to parse
        
    :material-location-enter: `default_value`
    :    Optional default value if conversion fails
        
    :material-keyboard-return: **Return**
    :    A JSON Object if successful, else a `std::nullopt`
    
    ```
    auto user = tryParseJsonStr("{\"name\": \"John Doe\"}");
    if(user.has_value()) {
         // Do something ...
    }
    ```
    

### verifyPassword<a name="verifyPassword"></a>
!!! function "bool verifyPassword(const std::string &amp;password, const std::string &amp;stored_hash)"

    Verifies user password if it matches the given hashed password.
    
    Given a hashed password from the database (`stored_hash`), the method extracts the salt value,
    hashes the `password` value with the salt then compares the two hashes if they match.
    
    
    :material-location-enter: `password`
    :    User password input.
        
    :material-location-enter: `stored_hash`
    :    Database stored hashed user password.
        
    :material-keyboard-return: **Return**
    :    boolean indicating whether the verification was successful or not.
    

