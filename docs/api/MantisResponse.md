---
generator: doxide
---


# MantisResponse

**class MantisResponse**

Wrapper around httplib::Response for consistent API.

Provides convenient methods for setting response status, headers, and body
with support for JSON, text, HTML, and file responses.

```cpp
// Send JSON response
res.sendJSON(200, {{"message", "Success"}});

// Send text response
res.sendText(200, "Hello World");

// Set custom header
res.setHeader("X-Custom", "value");
```


## Functions

| Name | Description |
| ---- | ----------- |
| [MantisResponse](#MantisResponse) | Construct response wrapper. |
| [getResponse](#getResponse) | Destructor. |
| [getStatus](#getStatus) | Get HTTP status code. |
| [setStatus](#setStatus) | Set HTTP status code. |
| [getVersion](#getVersion) | Get HTTP version. |
| [setVersion](#setVersion) | Set HTTP version. |
| [getBody](#getBody) | Get response body. |
| [setBody](#setBody) | Set response body. |
| [getLocation](#getLocation) | Get redirect location. |
| [setLocation](#setLocation) | Set redirect location. |
| [getReason](#getReason) | Get status reason phrase. |
| [setReason](#setReason) | Set status reason phrase. |
| [hasHeader](#hasHeader) | Check if header exists. |
| [getHeaderValue](#getHeaderValue) | Get header value. |
| [getHeaderValueU64](#getHeaderValueU64) | Get header value as unsigned 64-bit integer. |
| [getHeaderValueCount](#getHeaderValueCount) | Get count of header values. |
| [setHeader](#setHeader) | Set response header. |
| [hasTrailer](#hasTrailer) | Check if trailer exists. |
| [getTrailerValue](#getTrailerValue) | Get trailer value. |
| [getTrailerValueCount](#getTrailerValueCount) | Get count of trailer values. |
| [setRedirect](#setRedirect) | Set redirect response. |
| [setContent](#setContent) | Set response content from buffer. |
| [setContent](#setContent) | Set response content from string. |
| [setContent](#setContent) | Set response content from moved string. |
| [setFileContent](#setFileContent) | Set response content from file with MIME type. |
| [setFileContent](#setFileContent) | Set response content from file (auto-detect MIME type). |
| [send](#send) | Send response with status, data, and content type. |
| [sendJSON](#sendJSON) | Send JSON response. |
| [sendJson](#sendJson) | Send JSON response from Duktape value (for JavaScript). |
| [sendText](#sendText) | Send text response. |
| [sendHtml](#sendHtml) | Send HTML response. |
| [sendEmpty](#sendEmpty) | Send empty response (no body). |
| [registerDuktapeMethods](#registerDuktapeMethods) | Register MantisResponse methods for JavaScript/Duktape. |

## Function Details

### MantisResponse<a name="MantisResponse"></a>
!!! function "explicit MantisResponse(httplib::Response &amp;_resp)"

    Construct response wrapper.
    
    :material-location-enter: `_resp`
    :    Reference to httplib::Response object
    

### getBody<a name="getBody"></a>
!!! function "[[nodiscard]] std::string getBody() const"

    Get response body.
        
    :material-keyboard-return: **Return**
    :    Body string
    

### getHeaderValue<a name="getHeaderValue"></a>
!!! function "std::string getHeaderValue(const std::string &amp;key, const char &#42;def = &quot;&quot;, size_t id = 0) const"

    Get header value.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-location-enter: `def`
    :    Default value if not found
        
    :material-location-enter: `id`
    :    Index if multiple values (0-based)
        
    :material-keyboard-return: **Return**
    :    Header value or default
    

### getHeaderValueCount<a name="getHeaderValueCount"></a>
!!! function "[[nodiscard]] size_t getHeaderValueCount(const std::string &amp;key) const"

    Get count of header values.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### getHeaderValueU64<a name="getHeaderValueU64"></a>
!!! function "[[nodiscard]] size_t getHeaderValueU64(const std::string &amp;key, size_t def = 0, size_t id = 0) const"

    Get header value as unsigned 64-bit integer.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-location-enter: `def`
    :    Default value
        
    :material-location-enter: `id`
    :    Index if multiple values
        
    :material-keyboard-return: **Return**
    :    Header value as size_t
    

### getLocation<a name="getLocation"></a>
!!! function "[[nodiscard]] std::string getLocation() const"

    Get redirect location.
        
    :material-keyboard-return: **Return**
    :    Location URL
    

### getReason<a name="getReason"></a>
!!! function "[[nodiscard]] std::string getReason() const"

    Get status reason phrase.
        
    :material-keyboard-return: **Return**
    :    Reason string
    

### getResponse<a name="getResponse"></a>
!!! function "httplib::Response &amp;getResponse() const"

    Destructor.
    

### getStatus<a name="getStatus"></a>
!!! function "[[nodiscard]] int getStatus() const"

    Get HTTP status code.
        
    :material-keyboard-return: **Return**
    :    Status code (e.g., 200, 404, 500)
    

### getTrailerValue<a name="getTrailerValue"></a>
!!! function "[[nodiscard]] std::string getTrailerValue(const std::string &amp;key, size_t id = 0) const"

    Get trailer value.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-location-enter: `id`
    :    Index if multiple values
        
    :material-keyboard-return: **Return**
    :    Trailer value
    

### getTrailerValueCount<a name="getTrailerValueCount"></a>
!!! function "[[nodiscard]] size_t getTrailerValueCount(const std::string &amp;key) const"

    Get count of trailer values.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-keyboard-return: **Return**
    :    Number of values
    

### getVersion<a name="getVersion"></a>
!!! function "[[nodiscard]] std::string getVersion() const"

    Get HTTP version.
        
    :material-keyboard-return: **Return**
    :    Version string
    

### hasHeader<a name="hasHeader"></a>
!!! function "[[nodiscard]] bool hasHeader(const std::string &amp;key) const"

    Check if header exists.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-keyboard-return: **Return**
    :    true if header exists
    

### hasTrailer<a name="hasTrailer"></a>
!!! function "[[nodiscard]] bool hasTrailer(const std::string &amp;key) const"

    Check if trailer exists.
        
    :material-location-enter: `key`
    :    Trailer name
        
    :material-keyboard-return: **Return**
    :    true if trailer exists
    

### registerDuktapeMethods<a name="registerDuktapeMethods"></a>
!!! function "static void registerDuktapeMethods()"

    Register MantisResponse methods for JavaScript/Duktape.
    

### send<a name="send"></a>
!!! function "void send(int statusCode, const std::string &amp;data = &quot;&quot;, const std::string &amp;content_type = &quot;text/plain&quot;) const"

    Send response with status, data, and content type.
        
    :material-location-enter: `statusCode`
    :    HTTP status code
        
    :material-location-enter: `data`
    :    Response data
        
    :material-location-enter: `content_type`
    :    MIME type (default: "text/plain")
    

### sendEmpty<a name="sendEmpty"></a>
!!! function "void sendEmpty(int statusCode = 204) const"

    Send empty response (no body).
        
    :material-location-enter: `statusCode`
    :    HTTP status code (default: 204 No Content)
    

### sendHtml<a name="sendHtml"></a>
!!! function "void sendHtml(int statusCode = 200, const std::string &amp;data = &quot;&quot;) const"

    Send HTML response.
        
    :material-location-enter: `statusCode`
    :    HTTP status code (default: 200)
        
    :material-location-enter: `data`
    :    HTML content
    

### sendJSON<a name="sendJSON"></a>
!!! function "void sendJSON(int statusCode = 200, const json &amp;data = json::object()) const"

    Send JSON response.
        
    :material-location-enter: `statusCode`
    :    HTTP status code (default: 200)
        
    :material-location-enter: `data`
    :    JSON object to send
        
    ```
        res.sendJSON(200, {{"success", true}, {"data", result}});
        
    ```
    

### sendJson<a name="sendJson"></a>
!!! function "void sendJson(int statusCode, const DukValue &amp;data) const"

    Send JSON response from Duktape value (for JavaScript).
        
    :material-location-enter: `statusCode`
    :    HTTP status code
        
    :material-location-enter: `data`
    :    DukValue object
    

### sendText<a name="sendText"></a>
!!! function "void sendText(int statusCode = 200, const std::string &amp;data = &quot;&quot;) const"

    Send text response.
        
    :material-location-enter: `statusCode`
    :    HTTP status code (default: 200)
        
    :material-location-enter: `data`
    :    Text content
    

### setBody<a name="setBody"></a>
!!! function "void setBody(const std::string &amp;b)"

    Set response body.
        
    :material-location-enter: `b`
    :    Body string
    

### setContent<a name="setContent"></a>
!!! function "void setContent(const char &#42;s, size_t n, const std::string &amp;content_type) const"

    Set response content from buffer.
        
    :material-location-enter: `s`
    :    Content buffer
        
    :material-location-enter: `n`
    :    Content length
        
    :material-location-enter: `content_type`
    :    MIME type
    

!!! function "void setContent(const std::string &amp;s, const std::string &amp;content_type) const"

    Set response content from string.
        
    :material-location-enter: `s`
    :    Content string
        
    :material-location-enter: `content_type`
    :    MIME type
    

!!! function "void setContent(std::string &amp;&amp;s, const std::string &amp;content_type) const"

    Set response content from moved string.
        
    :material-location-enter: `s`
    :    Content string (moved)
        
    :material-location-enter: `content_type`
    :    MIME type
    

### setFileContent<a name="setFileContent"></a>
!!! function "void setFileContent(const std::string &amp;path, const std::string &amp;content_type) const"

    Set response content from file with MIME type.
        
    :material-location-enter: `path`
    :    File path
        
    :material-location-enter: `content_type`
    :    MIME type
    

!!! function "void setFileContent(const std::string &amp;path) const"

    Set response content from file (auto-detect MIME type).
        
    :material-location-enter: `path`
    :    File path
    

### setHeader<a name="setHeader"></a>
!!! function "void setHeader(const std::string &amp;key, const std::string &amp;val) const"

    Set response header.
        
    :material-location-enter: `key`
    :    Header name
        
    :material-location-enter: `val`
    :    Header value
    

### setLocation<a name="setLocation"></a>
!!! function "void setLocation(const std::string &amp;b)"

    Set redirect location.
        
    :material-location-enter: `b`
    :    Location URL
    

### setReason<a name="setReason"></a>
!!! function "void setReason(const std::string &amp;b)"

    Set status reason phrase.
        
    :material-location-enter: `b`
    :    Reason string
    

### setRedirect<a name="setRedirect"></a>
!!! function "void setRedirect(const std::string &amp;url, int status = httplib::StatusCode::Found_302) const"

    Set redirect response.
        
    :material-location-enter: `url`
    :    Redirect URL
        
    :material-location-enter: `status`
    :    HTTP status code (default: 302 Found)
    

### setStatus<a name="setStatus"></a>
!!! function "void setStatus(int s)"

    Set HTTP status code.
        
    :material-location-enter: `s`
    :    Status code
    

### setVersion<a name="setVersion"></a>
!!! function "void setVersion(const std::string &amp;b)"

    Set HTTP version.
        
    :material-location-enter: `b`
    :    Version string
    

