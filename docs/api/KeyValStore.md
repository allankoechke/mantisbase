---
generator: doxide
---


# KeyValStore

**class KeyValStore**

Manages application settings


## Functions

| Name | Description |
| ---- | ----------- |
| [setupRoutes](#setupRoutes) | Initialize and set up routes for fetching settings data :material-keyboard-return: **Return** :    `true` if setting up routes succeeded.  |
| [migrate](#migrate) | Initialize migration, create base data for setting fields  |
| [hasAccess](#hasAccess) | Evaluate if request is authenticated and has permission to access this route. |
| [configs](#configs) | Get the current config data instance. |
| [initSettingsConfig](#initSettingsConfig) | Fetch config data from database, initializing it to defaults if not existing yet! |
| [setupConfigRoutes](#setupConfigRoutes) | Called by :material-eye-outline: **See** :    setupRoutes() to initialize routes specific to settings config only!  |

## Function Details

### configs<a name="configs"></a>
!!! function "json&amp; configs()"

    Get the current config data instance.
    
    
    :material-keyboard-return: **Return**
    :    Config data as a JSON object
    

### hasAccess<a name="hasAccess"></a>
!!! function "HandlerResponse hasAccess([[maybe_unused]] MantisRequest&amp; req, MantisResponse&amp; res) const"

    Evaluate if request is authenticated and has permission to access this route.
    
    This route is exclusive to admin login only!
    
    
    :material-location-enter: `req`
    :    HTTP request
        
    :material-location-enter: `res`
    :    HTTP response
        
    :material-location-enter: `ctx`
    :    HTTP context
        
    :material-keyboard-return: **Return**
    :    `true` if access is granted, else, `false`
    

### initSettingsConfig<a name="initSettingsConfig"></a>
!!! function "json initSettingsConfig()"

    Fetch config data from database, initializing it to defaults if not existing yet!
    
    
    :material-keyboard-return: **Return**
    :    json object having the config, or empty json object.
    

### migrate<a name="migrate"></a>
!!! function "void migrate()"

    Initialize migration, create base data for setting fields
    

### setupConfigRoutes<a name="setupConfigRoutes"></a>
!!! function "void setupConfigRoutes()"

    Called by 
    :material-eye-outline: **See**
    :    setupRoutes() to initialize routes specific to settings config only!
    

### setupRoutes<a name="setupRoutes"></a>
!!! function "bool setupRoutes()"

    Initialize and set up routes for fetching settings data
    
    :material-keyboard-return: **Return**
    :    `true` if setting up routes succeeded.
    

