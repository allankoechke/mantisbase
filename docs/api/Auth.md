---
generator: doxide
---


# Auth

**class Auth**

JWT token creation and verification utilities.

Provides static methods for generating and validating JWT tokens
used for authentication and authorization.

```
// Create token for user
json claims = {{"id", "user123"}, {"table", "users"}};
std::string token = Auth::createToken(claims, 3600); // 1 hour

// Verify token
json result = Auth::verifyToken(token);
if (result["verified"].get<bool>()) {
    std::string userId = result["id"];
}
```


## Functions

| Name | Description |
| ---- | ----------- |
| [createToken](#createToken) | Create JWT token with custom claims. |
| [verifyToken](#verifyToken) | Verify JWT token and extract claims. |

## Function Details

### createToken<a name="createToken"></a>
!!! function "static std::string createToken(const json&amp; claims_params, int timeout = -1)"

    Create JWT token with custom claims.
    
    :material-location-enter: `claims_params`
    :    JSON object with claims (must include "id" and "table")
        
    :material-location-enter: `timeout`
    :    Token expiration in seconds (-1 for default, typically 1 hour)
        
    :material-keyboard-return: **Return**
    :    JWT token string
        ```
        json claims = {{"id", "user123"}, {"table", "users"}};
        std::string token = Auth::createToken(claims, 3600);
        ```
    

### verifyToken<a name="verifyToken"></a>
!!! function "static json verifyToken(const std::string&amp; token)"

    Verify JWT token and extract claims.
        
    :material-location-enter: `token`
    :    JWT token string to verify
        
    :material-keyboard-return: **Return**
    :    JSON object with:
          - "verified": bool indicating if token is valid
          - "id": user ID from token
          - "entity": entity table name from token
          - "error": error message if verification failed
        ```
        json result = Auth::verifyToken(token);
        if (result["verified"]) {
            // Token is valid, use result["id"] and result["entity"]
        }
        ```
    

