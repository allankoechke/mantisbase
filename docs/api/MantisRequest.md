---
generator: doxide
---


# MantisRequest

**class MantisRequest**

A wrapper class around `httplib::Request` offering a
consistent API and allowing for easy wrapper methods compatible
with `Duktape` API requirements for scripting.

Additionally, `MantisRequest` adds a context object for storing
some `key`-`value` data for sharing across middlewares and
request handlers.


## Functions

| Name | Description |
| ---- | ----------- |
| [MantisRequest](#MantisRequest) | Wrapper class around the httplib Request object and our context library. |
| [getMethod](#getMethod) | Get HTTP request method (GET, POST, etc.). :material-keyboard-return: **Return** :    Method string  |
| [getPath](#getPath) | Get request path. |
| [getBody](#getBody) | Get raw request body. |
| [getRemoteAddr](#getRemoteAddr) | Get client remote address. |
| [getRemotePort](#getRemotePort) | Get client remote port. |
| [getLocalAddr](#getLocalAddr) | Get server local address. |
| [getLocalPort](#getLocalPort) | Get server local port. |
| [hasHeader](#hasHeader) | Check if header exists. |
| [getHeaderValue](#getHeaderValue) | Get header value with default and index. |
| [getHeaderValueU64](#getHeaderValueU64) | Get header value as unsigned 64-bit integer. |
| [getHeaderValueCount](#getHeaderValueCount) | Get count of header values. |
| [hasTrailer](#hasTrailer) | Check if trailer exists. |
| [getTrailerValue](#getTrailerValue) | Get trailer value. |
| [getTrailerValueCount](#getTrailerValueCount) | Get count of trailer values. |
| [matches](#matches) | Get route match information. |
| [hasQueryParam](#hasQueryParam) | Check if query parameter exists. |
| [getQueryParamValue](#getQueryParamValue) | Get query parameter value (first occurrence). |
| [getQueryParamValue](#getQueryParamValue) | Get query parameter value by index. |
| [getQueryParamValueCount](#getQueryParamValueCount) | Get count of query parameter values. |
| [hasPathParams](#hasPathParams) | Check if request has path parameters. |
| [hasPathParam](#hasPathParam) | Check if path parameter exists. |
| [getPathParamValue](#getPathParamValue) | Get path parameter value. |
| [getPathParamValueCount](#getPathParamValueCount) | Get count of path parameter values. |
| [isMultipartFormData](#isMultipartFormData) | Check if request is multipart/form-data. |
| [registerDuktapeMethods](#registerDuktapeMethods) | Register MantisRequest methods for JavaScript/Duktape. |
| [hasKey](#hasKey) | Check if context store has key. |
| [getBearerTokenAuth](#getBearerTokenAuth) | Extract Bearer token from Authorization header. |
| [getBodyAsJson](#getBodyAsJson) | Parse request body as JSON. |
| [set](#set) | Store value in request context (for middleware communication). |
| [getOr](#getOr) | Get value from context store or return default. |

## Function Details

### MantisRequest<a name="MantisRequest"></a>
!!! function "explicit MantisRequest(const httplib::Request &amp;_req)"

    Wrapper class around the httplib Request object and
    our context library.
    
    Construct request wrapper.
    
    :material-location-enter: `_req`
    :    Reference to httplib::Request object
    

### getBearerTokenAuth<a name="getBearerTokenAuth"></a>
!!! function "std::string getBearerTokenAuth() const"

    Extract Bearer token from Authorization header.
        
    :material-keyboard-return: **Return**
    :    Token string (without "Bearer " prefix)
    

### getBody<a name="getBody"></a>
!!! function "std::string getBody() const"

    Get raw request body.
        
    :material-keyboard-return: **Return**
    :    Body string
    

### getBodyAsJson<a name="getBodyAsJson"></a>
!!! function "std::pair&lt;nlohmann::json, std::string&gt; getBodyAsJson() const"

    Parse request body as JSON.
        
    :material-keyboard-return: **Return**
    :    Pair of (JSON object, error message)
          - If parsing succeeds: (json object, "")
          - If parsing fails: (empty json, error message)
    

### getHeaderValue<a name="getHeaderValue"></a>
!!! function "std::string getHeaderValue(const std::string &amp;key, const char &#42;def, size_t id) const"

    Get header value with default and index.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-location-enter: `def`
    :    Default value if not found
        
    :material-location-enter: `id`
    :    Index if multiple values (0-based)
        
    :material-keyboard-return: **Return**
    :    Header value or default
    

### getHeaderValueCount<a name="getHeaderValueCount"></a>
!!! function "size_t getHeaderValueCount(const std::string &amp;key) const"

    Get count of header values.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### getHeaderValueU64<a name="getHeaderValueU64"></a>
!!! function "size_t getHeaderValueU64(const std::string &amp;key, size_t def, size_t id) const"

    Get header value as unsigned 64-bit integer.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-location-enter: `def`
    :    Default value
        
    :material-location-enter: `id`
    :    Index if multiple values
        
    :material-keyboard-return: **Return**
    :    Header value as size_t
    

### getLocalAddr<a name="getLocalAddr"></a>
!!! function "std::string getLocalAddr() const"

    Get server local address.
        
    :material-keyboard-return: **Return**
    :    IP address string
    

### getLocalPort<a name="getLocalPort"></a>
!!! function "int getLocalPort() const"

    Get server local port.
        
    :material-keyboard-return: **Return**
    :    Port number
    

### getMethod<a name="getMethod"></a>
!!! function "std::string getMethod() const"

    Get HTTP request method (GET, POST, etc.).
        
    :material-keyboard-return: **Return**
    :    Method string
    

### getOr<a name="getOr"></a>
!!! function "template&lt;typename T&gt; T &amp;getOr(const std::string &amp;key, T default_value)"

    Get value from context store or return default.
        
    :material-code-tags: `T`
    :    Value type
        
    :material-location-enter: `key`
    :    Context key
        
    :material-location-enter: `default_value`
    :    Default value if key not found
        
    :material-keyboard-return: **Return**
    :    Reference to value (or default if not found)
        
    ```
        int count = req.getOr<int>("count", 0);
        std::string name = req.getOr<std::string>("name", "Unknown");
        
    ```
    

### getPath<a name="getPath"></a>
!!! function "std::string getPath() const"

    Get request path.
        
    :material-keyboard-return: **Return**
    :    Path string
    

### getPathParamValue<a name="getPathParamValue"></a>
!!! function "std::string getPathParamValue(const std::string &amp;key) const"

    Get path parameter value.
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-keyboard-return: **Return**
    :    Parameter value
        
    ```
        // Route: GET /api/v1/users/:id
        // Request: GET /api/v1/users/123
        std::string id = req.getPathParamValue("id"); // "123"
        
    ```
    

### getPathParamValueCount<a name="getPathParamValueCount"></a>
!!! function "size_t getPathParamValueCount(const std::string &amp;key) const"

    Get count of path parameter values.
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### getQueryParamValue<a name="getQueryParamValue"></a>
!!! function "std::string getQueryParamValue(const std::string &amp;key) const"

    Get query parameter value (first occurrence).
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-keyboard-return: **Return**
    :    Parameter value or empty string
    

!!! function "std::string getQueryParamValue(const std::string &amp;key, size_t id) const"

    Get query parameter value by index.
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-location-enter: `id`
    :    Index if multiple values (0-based)
        
    :material-keyboard-return: **Return**
    :    Parameter value
    

### getQueryParamValueCount<a name="getQueryParamValueCount"></a>
!!! function "size_t getQueryParamValueCount(const std::string &amp;key) const"

    Get count of query parameter values.
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### getRemoteAddr<a name="getRemoteAddr"></a>
!!! function "std::string getRemoteAddr() const"

    Get client remote address.
        
    :material-keyboard-return: **Return**
    :    IP address string
    

### getRemotePort<a name="getRemotePort"></a>
!!! function "int getRemotePort() const"

    Get client remote port.
        
    :material-keyboard-return: **Return**
    :    Port number
    

### getTrailerValue<a name="getTrailerValue"></a>
!!! function "std::string getTrailerValue(const std::string &amp;key, size_t id) const"

    Get trailer value.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-location-enter: `id`
    :    Index if multiple values
        
    :material-keyboard-return: **Return**
    :    Trailer value
    

### getTrailerValueCount<a name="getTrailerValueCount"></a>
!!! function "size_t getTrailerValueCount(const std::string &amp;key) const"

    Get count of trailer values.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### hasHeader<a name="hasHeader"></a>
!!! function "bool hasHeader(const std::string &amp;key) const"

    Check if header exists.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-keyboard-return: **Return**
    :    true if header exists
    

### hasKey<a name="hasKey"></a>
!!! function "bool hasKey(const std::string &amp;key) const"

    Check if context store has key.
        
    :material-location-enter: `key`
    :    Key to check
        
    :material-keyboard-return: **Return**
    :    true if key exists in context
    

### hasPathParam<a name="hasPathParam"></a>
!!! function "bool hasPathParam(const std::string &amp;key) const"

    Check if path parameter exists.
        
    :material-location-enter: `key`
    :    Parameter name (from route pattern like /:id)
        
    :material-keyboard-return: **Return**
    :    true if parameter exists
    

### hasPathParams<a name="hasPathParams"></a>
!!! function "bool hasPathParams() const"

    Check if request has path parameters.
        
    :material-keyboard-return: **Return**
    :    true if path parameters exist
    

### hasQueryParam<a name="hasQueryParam"></a>
!!! function "bool hasQueryParam(const std::string &amp;key) const"

    Check if query parameter exists.
        
    :material-location-enter: `key`
    :    Parameter name
        
    :material-keyboard-return: **Return**
    :    true if parameter exists
    

### hasTrailer<a name="hasTrailer"></a>
!!! function "bool hasTrailer(const std::string &amp;key) const"

    Check if trailer exists.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-keyboard-return: **Return**
    :    true if trailer exists
    

### isMultipartFormData<a name="isMultipartFormData"></a>
!!! function "bool isMultipartFormData() const"

    Check if request is multipart/form-data.
        
    :material-keyboard-return: **Return**
    :    true if multipart form data
    

### matches<a name="matches"></a>
!!! function "httplib::Match matches() const"

    Get route match information.
        
    :material-keyboard-return: **Return**
    :    httplib::Match object with path parameters
    

### registerDuktapeMethods<a name="registerDuktapeMethods"></a>
!!! function "static void registerDuktapeMethods()"

    Register MantisRequest methods for JavaScript/Duktape.
    

### set<a name="set"></a>
!!! function "template&lt;typename T&gt; void set(const std::string &amp;key, T value)"

    Store value in request context (for middleware communication).
        
    :material-code-tags: `T`
    :    Value type
        
    :material-location-enter: `key`
    :    Context key
        
    :material-location-enter: `value`
    :    Value to store
        
    ```
        req.set<std::string>("user_id", "123");
        req.set<int>("count", 42);
        
    ```
    

