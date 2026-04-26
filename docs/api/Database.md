---
generator: doxide
---


# Database

**class Database**

Database connection and session management.

Handles database connections, connection pooling, and provides
session management for executing queries. Supports SQLite (default)
and PostgreSQL.

```cpp
Database db;
db.connect("dbname=mantis user=postgres password=pass");
auto session = db.session();
*session << "SELECT * FROM users", soci::into(rows);
```


## Functions

| Name | Description |
| ---- | ----------- |
| [Database](#Database) | Construct database instance. |
| [~Database](#_u007eDatabase) | Destructor (disconnects from database). |
| [connect](#connect) | Connect to database and initialize connection pool. |
| [disconnect](#disconnect) | Close all database connections and destroy connection pool. |
| [createSysTables](#createSysTables) | Create system tables (mb_tables, mb_admins, etc.). :material-keyboard-return: **Return** :    true if migration successful, false otherwise  |
| [session](#session) | Get a database session from the connection pool. |
| [connectionPool](#connectionPool) | Get access to the underlying connection pool. |
| [isConnected](#isConnected) | Check if database is connected. |
| [query](#query) | Execute an SQL script, bound to any given values and return a single or no Dukvalue object. |
| [writeCheckpoint](#writeCheckpoint) | Write WAL data to db file and truncate the WAL file  |

## Function Details

### Database<a name="Database"></a>
!!! function "Database()"

    Construct database instance.
    

### connect<a name="connect"></a>
!!! function "bool connect(const std::string &amp;conn_str)"

    Connect to database and initialize connection pool.
    
    :material-location-enter: `conn_str`
    :    Connection string (format depends on database type)
          - SQLite: path to database file or empty for default
          - PostgreSQL: "dbname=name host=host port=5432 user=user password=pass"
        
    :material-keyboard-return: **Return**
    :    true if connection successful, false otherwise
    

### connectionPool<a name="connectionPool"></a>
!!! function "[[nodiscard]] soci::connection_pool &amp;connectionPool() const"

    Get access to the underlying connection pool.
        
    :material-keyboard-return: **Return**
    :    Reference to soci::connection_pool
    

### createSysTables<a name="createSysTables"></a>
!!! function "bool createSysTables() const"

    Create system tables (mb_tables, mb_admins, etc.).
        
    :material-keyboard-return: **Return**
    :    true if migration successful, false otherwise
    

### disconnect<a name="disconnect"></a>
!!! function "void disconnect()"

    Close all database connections and destroy connection pool.
    

### isConnected<a name="isConnected"></a>
!!! function "[[nodiscard]] bool isConnected() const"

    Check if database is connected.
        
    :material-keyboard-return: **Return**
    :    true if connected, false otherwise
    

### query<a name="query"></a>
!!! function "duk_ret_t query(duk_context &#42;ctx)"

    Execute an SQL script, bound to any given values and return a single or no
        Dukvalue object. In case of any error, we throw the error back to JS.
    
    
    ```
    const user = query("SELECT * FROM students WHERE id = :id LIMIT 1", {id: "12345643"})
    if(user!==undefined) {
        // ....
    }
    query("INSERT INTO students(name, age) VALUES ()", {name: "John Doe"}, {age: 12})
    
    ```
    
    
    :material-location-enter: `ctx`
    :    duktape context object
        
    :material-keyboard-return: **Return**
    :    Execution status of the query. The objects returned (JSON Object or JSON Array) are
        pushed directly to the JS context
    

### session<a name="session"></a>
!!! function "[[nodiscard]] std::shared_ptr&lt;soci::session&gt; session() const"

    Get a database session from the connection pool.
        
    :material-keyboard-return: **Return**
    :    Shared pointer to soci::session
        
    ```
        auto sql = db.session();
        *sql << "SELECT * FROM users WHERE id = :id", soci::use(id), soci::into(row);
        
    ```
    

### writeCheckpoint<a name="writeCheckpoint"></a>
!!! function "void writeCheckpoint() const"

    Write WAL data to db file and truncate the WAL file
    

### ~Database<a name="_u007eDatabase"></a>
!!! function "~Database()"

    Destructor (disconnects from database).
    

