---
generator: doxide
---


# MantisBase

**class MantisBase**

MantisBase entry point.

This class handles the entrypoint to the `Mantis` world, where we can
set/get application flags and variables, as well as access other
application units. These units are:
- DatabaseMgr: For all database handling, 
:material-eye-outline: **See**
:    DatabaseMgr for more information.
- HttpMgr: Low-level http server operation and routing. 
:material-eye-outline: **See**
:    HttpMgr for low-level access or 
:material-eye-outline: **See**
:    RouterMgr for a high level routing methods.
- LogsMgr: For logging capabilities, 
:material-eye-outline: **See**
:    LoggingMgr for more details.
- RouterMgr: High level routing wrapper on top of 
:material-eye-outline: **See**
:    HttpMgr, 
:material-eye-outline: **See**
:    RouterMgr for more details.
- ValidatorMgr: A validation store using regex, 
:material-eye-outline: **See**
:    Validator for more details.


## Functions

| Name | Description |
| ---- | ----------- |
| [~MantisBase](#_u007eMantisBase) | Disable copying and moving  |
| [instance](#instance) | Retrieve existing application instance. |
| [create](#create) | Create class instance given cmd args passed in. |
| [create](#create) | Convenience function to allow creating class instance given the values needed to set up the app without any need for passing in cmd args. |
| [run](#run) | Start the http server and start listening for requests. |
| [close](#close) | Close the application and reset object instances that are dependent on the class Internally, this stops running http server, disconnects from the database and does any required cleanup  |
| [quit](#quit) | Quit the running application immediately. |
| [port](#port) | Retrieve HTTP Listening port. |
| [setPort](#setPort) | Set a new port value for HTTP server :material-location-enter: `port` :    New HTTP port value.  |
| [poolSize](#poolSize) | Retrieve the database pool size value. |
| [host](#host) | Retrieve HTTP Server host address. |
| [setHost](#setHost) | Update HTTP Server host address. |
| [publicDir](#publicDir) | Retrieve the public static file directory. |
| [setPublicDir](#setPublicDir) | Update HTTP server static file directory. |
| [dataDir](#dataDir) | Retrieves the data directory where SQLite db and files are stored. |
| [setDataDir](#setDataDir) | Update the data directory for MantisApp. |
| [scriptsDir](#scriptsDir) | Retrieves the scripts directory where JavaScript files are stored used for extending functionality in Mantis. |
| [setScriptsDir](#setScriptsDir) | Update the scripts directory for MantisApp. |
| [dbType](#dbType) | Retrieves the active database type. |
| [setDbType](#setDbType) | Update the active database type for Mantis. |
| [jwtSecretKey](#jwtSecretKey) | Retrieve the JWT secret key. |
| [appVersion](#appVersion) | Fetch the application version :material-keyboard-return: **Return** :    Application version  |
| [appMinorVersion](#appMinorVersion) |     Fetch the major version  |
| [appMajorVersion](#appMajorVersion) |     Fetch the minor version  |
| [appPatchVersion](#appPatchVersion) |     Fetch the patch version  |
| [db](#db) |     Get the database unit object  |
| [cmd](#cmd) |     Get the commandline parser object  |
| [router](#router) |     Get the router object instance. |
| [settings](#settings) |     Get the KeyValue unit object  |
| [logs](#logs) |     Get the logs unit object  |
| [rt](#rt) |     Get the realtime unit (SQLite/PostgreSQL change detection for SSE /api/v1/realtime). |
| [entity](#entity) | Fetch a table schema encapsulated by an `Entity` object from given the table name. |
| [hasEntity](#hasEntity) | Check if table schema encapsulated by an `Entity` object from given the table name exists. |
| [ctx](#ctx) |     Get the duktape context  |
| [openBrowserOnStart](#openBrowserOnStart) | Launch browser with the admin dashboard page. |
| [startTime](#startTime) | Get the server start time in std::chrono:: :material-keyboard-return: **Return** :    Server start time  |
| [init](#init) | Run initialization actions for Mantis, ensuring all objects are initialized properly before use. |
| [getInstanceImpl](#getInstanceImpl) | Creates static instance if not created yet and returns it. |
| [setPoolSize](#setPoolSize) | Set the database pool size value. |
| [init_units](#init_units) |     > Parse command-line arguments  |
| [ensureDirsAreCreated](#ensureDirsAreCreated) |     > Initialize application units  |
| [initJSEngine](#initJSEngine) |     Ensures we created all required directories Initialize JS engine and register Mantis functions to JS  |
| [loadStartScript](#loadStartScript) | Load startup `.js` file `index.mantis.js` from the mantis scripts directory. |
| [loadAndExecuteScript](#loadAndExecuteScript) | Load and execute a passed in file path for a `.js` file. |
| [loadScript](#loadScript) | Load a script file and execute it :material-location-enter: `relativePath` :    Relative file path to be loaded.  |
| [duk_db](#duk_db) | Wrapper method to return `DatabaseUnit*` instead of `DatabaseUnit&` returned by :material-eye-outline: **See** :    db() method. :material-keyboard-return: **Return** :    DatabaseUnit instance pointer  |

## Function Details

### appMajorVersion<a name="appMajorVersion"></a>
!!! function "static int appMajorVersion()"

        Fetch the minor version
    

### appMinorVersion<a name="appMinorVersion"></a>
!!! function "static int appMinorVersion()"

        Fetch the major version
    

### appPatchVersion<a name="appPatchVersion"></a>
!!! function "static int appPatchVersion()"

        Fetch the patch version
    

### appVersion<a name="appVersion"></a>
!!! function "static std::string appVersion()"

    Fetch the application version
        
    :material-keyboard-return: **Return**
    :    Application version
    

### close<a name="close"></a>
!!! function "void close()"

    Close the application and reset object
        instances that are dependent on the class
    
    Internally, this stops running http server,
    disconnects from the database and does any required
    cleanup
    

### cmd<a name="cmd"></a>
!!! function "[[nodiscard]] argparse::ArgumentParser&amp; cmd() const"

        Get the commandline parser object
    

### create<a name="create"></a>
!!! function "static MantisBase&amp; create(int argc, char&#42;&#42; argv)"

    Create class instance given cmd args passed in.
    
    :material-eye-outline: **See**
    :    parseArgs() for expected cmd args to be passed in.
    
    
    :material-location-enter: `argc`
    :    Number of cmd args
        
    :material-location-enter: `argv`
    :    Char array list
        
    :material-keyboard-return: **Return**
    :    Reference to the created class instance
    

!!! function "static MantisBase&amp; create(const json&amp; config = json::object())"

    Convenience function to allow creating class instance given the
        values needed to set up the app without any need for passing in cmd args.
    
    The expected values are:
    {
        "database": "<db type>",
        "connection": "<connection string>",
        "dataDir": "<path to dir>",
        "publicDir": "<path to dir>",
        "scriptsDir": "<path to dir>",
        "dev": true,
        "serve": {
            "port": <int>,
            "host": "<host IP/addr>",
            "poolSize": <int>,
        },
        "admins": {
            "add": "<email to add>",
            "rm": "<email to remove>"
        }
    }
    
    
    !!! note
     All primary options above are optional, you can omit to use default values.
        
    !!! note
     `admins` subcommand expects a subcommand with either the `add` or `rm`.
            
    !!! note
     `serve` command can have an empty json object and the app will configure with defaults.
    
    
    ```
            json arg1 = json::object();
            auto& app1 = MantisApp::create(arg2);
    
    json arg2;
        arg2["database"] = "PSQL";
        arg2["database"] = "dbname=mantis username=postgres password=12342532";
        arg2["dev"] = true;
        arg2["serve"] = json::object{};
        auto& app2 = MantisApp::create(arg2);
    
    
    ```
    
    
    :material-location-enter: `config`
    :    JSON Object bearing the cmd args values to be used
        
    :material-keyboard-return: **Return**
    :    A reference to the created class instance
    

### ctx<a name="ctx"></a>
!!! function "[[nodiscard]] duk_context&#42; ctx() const"

        Get the duktape context
    

### dataDir<a name="dataDir"></a>
!!! function "[[nodiscard]] std::string dataDir() const"

    Retrieves the data directory where SQLite db and files are stored.
        
    :material-keyboard-return: **Return**
    :    MantisApp data directory.
    

### db<a name="db"></a>
!!! function "[[nodiscard]] Database&amp; db() const"

        Get the database unit object
    

### dbType<a name="dbType"></a>
!!! function "[[nodiscard]] std::string dbType() const"

    Retrieves the active database type.
        
    :material-keyboard-return: **Return**
    :    Selected DatabaseType enum value.
    

### duk_db<a name="duk_db"></a>
!!! function "[[nodiscard]] Database&#42; duk_db() const"

    Wrapper method to return `DatabaseUnit*` instead of
        `DatabaseUnit&` returned by 
    :material-eye-outline: **See**
    :    db() method.
    
    
    :material-keyboard-return: **Return**
    :    DatabaseUnit instance pointer
    

### ensureDirsAreCreated<a name="ensureDirsAreCreated"></a>
!!! function "[[nodiscard]] bool ensureDirsAreCreated() const"

        > Initialize application units
    

### entity<a name="entity"></a>
!!! function "[[nodiscard]] Entity entity(const std::string&amp; entity_name) const"

    Fetch a table schema encapsulated by an `Entity` object from given the table name.
        If table does not exist yet, return an emty object.
    
    
    :material-location-enter: `entity_name`
    :    Name of the table of interest
        
    :material-keyboard-return: **Return**
    :    Entity object for the selected table
    

### getInstanceImpl<a name="getInstanceImpl"></a>
!!! function "static MantisBase&amp; getInstanceImpl()"

    Creates static instance if not created yet and returns it.
    
    
    :material-keyboard-return: **Return**
    :    instance of the MantisApp class
    

### hasEntity<a name="hasEntity"></a>
!!! function "[[nodiscard]] bool hasEntity(const std::string&amp; entity_name) const"

    Check if table schema encapsulated by an `Entity` object from given the table name exists.
        If table does not exist yet, return false.
    
    
    :material-location-enter: `entity_name`
    :    Name of the table of interest
        
    :material-keyboard-return: **Return**
    :    true if entity exists, false otherwise.
    

### host<a name="host"></a>
!!! function "[[nodiscard]] std::string host() const"

    Retrieve HTTP Server host address. For instance, a host of `127.0.0.1`, `0.0.0.0`, etc.
        
    :material-keyboard-return: **Return**
    :    HTTP Server Host address.
    

### init<a name="init"></a>
!!! function "void init(int argc = 0, char&#42; argv[] = {})"

    Run initialization actions for Mantis, ensuring all objects are initialized properly before use.
    

### initJSEngine<a name="initJSEngine"></a>
!!! function "void initJSEngine()"

        Ensures we created all required directories
    Initialize JS engine and register Mantis functions to JS
    

### init_units<a name="init_units"></a>
!!! function "void init_units()"

        > Parse command-line arguments
    

### instance<a name="instance"></a>
!!! function "static MantisBase&amp; instance()"

    Retrieve existing application instance.
    
    :material-keyboard-return: **Return**
    :    A reference to the existing application instance.
    

### jwtSecretKey<a name="jwtSecretKey"></a>
!!! function "static std::string jwtSecretKey()"

    Retrieve the JWT secret key.
        
    :material-keyboard-return: **Return**
    :    JWT Secret value.
    

### loadAndExecuteScript<a name="loadAndExecuteScript"></a>
!!! function "void loadAndExecuteScript(const std::string&amp; filePath) const"

    Load and execute a passed in file path for a `.js` file.
    
    
    :material-location-enter: `filePath`
    :    File path to load and execute
    

### loadScript<a name="loadScript"></a>
!!! function "void loadScript(const std::string&amp; relativePath) const"

    Load a script file and execute it
    
    
    :material-location-enter: `relativePath`
    :    Relative file path to be loaded.
    

### loadStartScript<a name="loadStartScript"></a>
!!! function "void loadStartScript() const"

    Load startup `.js` file `index.mantis.js` from the mantis
        scripts directory.
    

### logs<a name="logs"></a>
!!! function "[[nodiscard]] Logger&amp; logs() const"

        Get the logs unit object
    

### openBrowserOnStart<a name="openBrowserOnStart"></a>
!!! function "void openBrowserOnStart() const"

    Launch browser with the admin dashboard page. If all goes well, the default
        OS browser should open (if not opened) with the admin dashboard URL.
    
    > Added in v0.1.6
    

### poolSize<a name="poolSize"></a>
!!! function "[[nodiscard]] int poolSize() const"

    Retrieve the database pool size value.
        
    :material-keyboard-return: **Return**
    :    SOCI's database pool size.
    

### port<a name="port"></a>
!!! function "[[nodiscard]] int port() const"

    Retrieve HTTP Listening port.
        
    :material-keyboard-return: **Return**
    :    Http Listening Port.
    

### publicDir<a name="publicDir"></a>
!!! function "[[nodiscard]] std::string publicDir() const"

    Retrieve the public static file directory.
        
    :material-keyboard-return: **Return**
    :    MantisApp public directory.
    

### quit<a name="quit"></a>
!!! function "static int quit(const int&amp; exitCode = 0, const std::string&amp; reason = &quot;Something went wrong!&quot;)"

    Quit the running application immediately.
    
    :material-location-enter: `exitCode`
    :    Exit code value
        
    :material-location-enter: `reason`
    :    User-friendly reason for the exit.
        
    :material-keyboard-return: **Return**
    :    `exitCode` value.
    

### router<a name="router"></a>
!!! function "[[nodiscard]] Router&amp; router() const"

        Get the router object instance.
    

### rt<a name="rt"></a>
!!! function "[[nodiscard]] RealtimeDB&amp; rt() const"

        Get the realtime unit (SQLite/PostgreSQL change detection for SSE /api/v1/realtime).
    

### run<a name="run"></a>
!!! function "[[nodiscard]] int run()"

    Start the http server and start listening for requests.
        
    :material-keyboard-return: **Return**
    :    `0` if execution was okay, else a non-zero value.
    

### scriptsDir<a name="scriptsDir"></a>
!!! function "[[nodiscard]] std::string scriptsDir() const"

    Retrieves the scripts directory where JavaScript files are stored
        used for extending functionality in Mantis.
    
    
    :material-keyboard-return: **Return**
    :    MantisApp scripts directory.
    

### setDataDir<a name="setDataDir"></a>
!!! function "void setDataDir(const std::string&amp; dir)"

    Update the data directory for MantisApp.
        
    :material-location-enter: `dir`
    :    New data directory.
    

### setDbType<a name="setDbType"></a>
!!! function "void setDbType(const std::string&amp; dbType)"

    Update the active database type for Mantis.
        
    :material-location-enter: `dbType`
    :    New database type enum value.
    

### setHost<a name="setHost"></a>
!!! function "void setHost(const std::string&amp; host)"

    Update HTTP Server host address.
        
    :material-location-enter: `host`
    :    New HTTP Server host address.
    

### setPoolSize<a name="setPoolSize"></a>
!!! function "void setPoolSize(const int&amp; pool_size)"

    Set the database pool size value.
    
    :material-location-enter: `pool_size`
    :    New pool size value.
    

### setPort<a name="setPort"></a>
!!! function "void setPort(const int&amp; port)"

    Set a new port value for HTTP server
        
    :material-location-enter: `port`
    :    New HTTP port value.
    

### setPublicDir<a name="setPublicDir"></a>
!!! function "void setPublicDir(const std::string&amp; dir)"

    Update HTTP server static file directory.
        
    :material-location-enter: `dir`
    :    New directory path.
    

### setScriptsDir<a name="setScriptsDir"></a>
!!! function "void setScriptsDir(const std::string&amp; dir)"

    Update the scripts directory for MantisApp.
    
    :material-location-enter: `dir`
    :    New scripts directory.
    

### settings<a name="settings"></a>
!!! function "[[nodiscard]] KeyValStore&amp; settings() const"

        Get the KeyValue unit object
    

### startTime<a name="startTime"></a>
!!! function "[[nodiscard]] std::chrono::time_point&lt;std::chrono::steady_clock&gt; startTime() const"

    Get the server start time in std::chrono::
    
    
    :material-keyboard-return: **Return**
    :    Server start time
    

### ~MantisBase<a name="_u007eMantisBase"></a>
!!! function "~MantisBase()"

    Disable copying and moving
    

