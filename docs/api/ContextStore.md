---
generator: doxide
---


# ContextStore

**class ContextStore**

The `ContextStore` class provides a means to set/get a key-value data that can be shared uniquely between middlewares
and the handler functions. This allows sending data down the chain from the first to the last handler.

For instance, the auth middleware will inject user `id` and subsequent middlewares can retrieve it as needed.

```cpp
// Create the object
Context ctx;

// Add values
ctx.set<std::string>("key", "Value");
ctx.set<int>("id", 967567);
ctx.set<bool>("verified", true);

// Retrieve values

// From scripting using JS
req.set("key", 5") // INT/DOUBLE/FLOATS ...
req.set("key2", { a: 5, b: 7}) // Objects/JSON
req.set("valid", true) // BOOLs

req.getOr("nothing", "Default Value")
```

The value returned from the `get()` is a std::optional, meaning a std::nullopt if the key was not found.
```cpp
std::optional key = ctx.get<std::string>("key");
if(key.has_value()) { .... }
```

Additionally, we have a 
:material-eye-outline: **See**
:    get_or() method that takes in a key and a default value if the key is missing. This
unlike 
:material-eye-outline: **See**
:    get() method, returns a `T&` instead of `T*` depending on the usage needs.


## Functions

| Name | Description |
| ---- | ----------- |
| [dump](#dump) | Convenience method for dumping context data for debugging. |
| [hasKey](#hasKey) |  :material-location-enter: `key` :    Key to check in context store :material-keyboard-return: **Return** :    `true` if key exists, else, `false`  |
| [set](#set) | Store a key-value data in the context :material-code-tags: `T` :    Value data type :material-location-enter: `key` :    Value key :material-location-enter: `value` :    Value to be stored  |
| [get](#get) | Get context value given the key. |
| [getOr](#getOr) | Get context value given the key. |
| [getOr_duk](#getOr_duk) | Get value from context store if available or return default_value instead. |
| [set_duk](#set_duk) | Store a value in store with given `key` and `value` :material-location-enter: `key` :    Key to store in context store :material-location-enter: `value` :    Value to correspond to given `key`  |

## Function Details

### dump<a name="dump"></a>
!!! function "void dump()"

    Convenience method for dumping context data for debugging.
    

### get<a name="get"></a>
!!! function "template &lt;typename T&gt; std::optional&lt;T&#42;&gt; get(const std::string&amp; key)"

    Get context value given the key.
    
    
    :material-code-tags: `T`
    :    Value data type
        
    :material-location-enter: `key`
    :    Value key
        
    :material-keyboard-return: **Return**
    :    Value wrapped in a std::optional
    

### getOr<a name="getOr"></a>
!!! function "template &lt;typename T&gt; T&amp; getOr(const std::string&amp; key, T default_value)"

    Get context value given the key.
    
    
    :material-code-tags: `T`
    :    Value data type
        
    :material-location-enter: `key`
    :    Value key
        
    :material-location-enter: `default_value`
    :    Default value if key is missing
        
    :material-keyboard-return: **Return**
    :    Value or default value
    

### getOr_duk<a name="getOr_duk"></a>
!!! function "DukValue getOr_duk(const std::string&amp; key, DukValue default_value)"

    Get value from context store if available or return default_value instead.
    
    
    :material-location-enter: `key`
    :    Key to check in the store
        
    :material-location-enter: `default_value`
    :    Default value to pass if missing
        
    :material-keyboard-return: **Return**
    :    Value of type `DukValue` corresponding to found value or default value
    

### hasKey<a name="hasKey"></a>
!!! function "bool hasKey(const std::string&amp; key) const"

    
    :material-location-enter: `key`
    :    Key to check in context store
        
    :material-keyboard-return: **Return**
    :    `true` if key exists, else, `false`
    

### set<a name="set"></a>
!!! function "template &lt;typename T&gt; void set(const std::string&amp; key, T value)"

    Store a key-value data in the context
    
    
    :material-code-tags: `T`
    :    Value data type
        
    :material-location-enter: `key`
    :    Value key
        
    :material-location-enter: `value`
    :    Value to be stored
    

### set_duk<a name="set_duk"></a>
!!! function "void set_duk(const std::string&amp; key, const DukValue&amp; value)"

    Store a value in store with given `key` and `value`
    
    
    :material-location-enter: `key`
    :    Key to store in context store
        
    :material-location-enter: `value`
    :    Value to correspond to given `key`
    

