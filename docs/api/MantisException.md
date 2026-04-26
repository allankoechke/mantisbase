---
generator: doxide
---


# MantisException

**class MantisException final : public std::exception**

Custom exception class for MantisBase errors.

Provides a structured exception type with error code, message,
and optional description for error handling.


## Functions

| Name | Description |
| ---- | ----------- |
| [MantisException](#MantisException) | Construct a MantisException with an error code and message. |
| [MantisException](#MantisException) | Construct a MantisException with an error code, message, and description. |
| [what](#what) | Get the error message. |
| [desc](#desc) | Get the error description. |
| [code](#code) | Get the error code. |

## Function Details

### MantisException<a name="MantisException"></a>
!!! function "MantisException(int _code, std::string _msg)"

    Construct a MantisException with an error code and message.
    
    :material-location-enter: `_code`
    :    Error code
        
    :material-location-enter: `_msg`
    :    Error message
    

!!! function "MantisException(int _code, std::string _msg, std::string _desc)"

    Construct a MantisException with an error code, message, and description.
        
    :material-location-enter: `_code`
    :    Error code
        
    :material-location-enter: `_msg`
    :    Error message
        
    :material-location-enter: `_desc`
    :    Error description
    

### code<a name="code"></a>
!!! function "[[nodiscard]] int code() const noexcept"

    Get the error code.
        
    :material-keyboard-return: **Return**
    :    Error code
    

### desc<a name="desc"></a>
!!! function "[[nodiscard]] const char&#42; desc() const noexcept"

    Get the error description.
        
    :material-keyboard-return: **Return**
    :    Error description
    

### what<a name="what"></a>
!!! function "[[nodiscard]] const char&#42; what() const noexcept override"

    Get the error message.
        
    :material-keyboard-return: **Return**
    :    Error message
    

