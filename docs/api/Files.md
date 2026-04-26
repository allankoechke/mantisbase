---
generator: doxide
---


# Files

**class Files**

File management for entity file assets.

Handles file storage and retrieval for entities with `file` or `files` field types.
Files are stored on disk in entity-specific directories, with filenames saved to the database.

Security features:
- Entity name validation to prevent SQL injection
- Path canonicalization to prevent directory traversal attacks
- Automatic sanitization of filenames

The class handles:
- Creating and deleting entity directories
- Creating and deleting file resources
- Updating folder names when entities are renamed
- Deleting folder contents when entities are dropped

```cpp
// Create directory for an entity
Files::createDir("products");

// Get file path (validates entity name and prevents path traversal)
std::string path = Files::filePath("products", "image.jpg");

// Check if file exists
if (Files::fileExists("products", "image.jpg")) {
    // File exists
}
```


## Functions

| Name | Description |
| ---- | ----------- |
| [createDir](#createDir) | Create a directory for the given entity. |
| [renameDir](#renameDir) | Rename an entity directory. |
| [deleteDir](#deleteDir) | Delete an entity directory and all its contents. |
| [dirPath](#dirPath) | Get the directory path for an entity. |
| [filePath](#filePath) | Get the absolute file path for an entity and filename. |
| [getFilePath](#getFilePath) | Get file path only if the file exists. |
| [removeFile](#removeFile) | Remove a file from an entity's directory. |
| [removeFiles](#removeFiles) | Remove multiple files from an entity's directory. |
| [fileExists](#fileExists) | Check if a file exists in an entity's directory. |
| [getCanonicalPath](#getCanonicalPath) | Get canonical path and verify it's within the files base directory. |
| [isCanonicalPath](#isCanonicalPath) | Check if a path is canonical and within the files base directory. |
| [filesBaseDir](#filesBaseDir) | Get the base directory for all entity files. |

## Function Details

### createDir<a name="createDir"></a>
!!! function "static void createDir(const std::string&amp; entity_name)"

    Create a directory for the given entity.
    
    Validates the entity name and creates the directory if it doesn't exist.
    Throws MantisException if entity name is invalid.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name (must be valid alphanumeric + underscore)
        @throws MantisException if entity name is invalid
        ```cpp
        Files::createDir("products");
        ```
    

### deleteDir<a name="deleteDir"></a>
!!! function "static void deleteDir(const std::string&amp; entity_name)"

    Delete an entity directory and all its contents.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name
    

### dirPath<a name="dirPath"></a>
!!! function "static std::string dirPath(const std::string&amp; entity_name, bool create_if_missing = false)"

    Get the directory path for an entity.
    
    Returns the absolute path to the entity's file directory. Validates entity name
    and checks for path traversal attacks before returning the path.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name (must be valid)
        
    :material-location-enter: `create_if_missing`
    :    If true, creates the directory if it doesn't exist
        
    :material-keyboard-return: **Return**
    :    Absolute path string to the entity's file directory
        @throws MantisException if entity name is invalid or path traversal is detected
    

### fileExists<a name="fileExists"></a>
!!! function "static bool fileExists(const std::string&amp; entity_name, const std::string&amp; filename)"

    Check if a file exists in an entity's directory.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name
        
    :material-location-enter: `filename`
    :    File name to check
        
    :material-keyboard-return: **Return**
    :    true if file exists, false otherwise
    

### filePath<a name="filePath"></a>
!!! function "static std::string filePath(const std::string&amp; entity_name, const std::string&amp; filename)"

    Get the absolute file path for an entity and filename.
    
    Constructs and canonicalizes the file path, ensuring it's within the entity's
    directory to prevent path traversal attacks. Validates entity name and filename.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name (must be valid)
        
    :material-location-enter: `filename`
    :    File name (must not be empty)
        
    :material-keyboard-return: **Return**
    :    Absolute canonical path string to the file
        @throws MantisException if entity name is invalid, filename is empty, or path traversal is detected
    

### filesBaseDir<a name="filesBaseDir"></a>
!!! function "static fs::path filesBaseDir()"

    Get the base directory for all entity files.
    
    Returns the path to the files directory (typically `<dataDir>/files`).
    
    
    :material-keyboard-return: **Return**
    :    Filesystem path to the base files directory
    

### getCanonicalPath<a name="getCanonicalPath"></a>
!!! function "static fs::path getCanonicalPath(const fs::path&amp; path)"

    Get canonical path and verify it's within the files base directory.
    
    Canonicalizes the given path (resolves symlinks, removes "..", etc.) and verifies
    that it's within the files base directory to prevent path traversal attacks.
    
    
    :material-location-enter: `path`
    :    Path to canonicalize and validate
        
    :material-keyboard-return: **Return**
    :    Canonical path if valid
        @throws MantisException if path traversal is detected (400 status)
    

### getFilePath<a name="getFilePath"></a>
!!! function "static std::optional&lt;std::string&gt; getFilePath(const std::string&amp; entity_name, const std::string&amp; filename)"

    Get file path only if the file exists.
    
    Returns the file path if the file exists on disk, otherwise returns std::nullopt.
    Validates entity name before checking file existence.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name
        
    :material-location-enter: `filename`
    :    File name
        
    :material-keyboard-return: **Return**
    :    Absolute file path if file exists, std::nullopt otherwise
        @throws MantisException if entity name is invalid
    

### isCanonicalPath<a name="isCanonicalPath"></a>
!!! function "static bool isCanonicalPath(const fs::path&amp; path)"

    Check if a path is canonical and within the files base directory.
    
    Canonicalizes the path and checks if it's within the files base directory
    without throwing an exception.
    
    
    :material-location-enter: `path`
    :    Path to check
        
    :material-keyboard-return: **Return**
    :    true if path is canonical and within base directory, false otherwise
    

### removeFile<a name="removeFile"></a>
!!! function "static bool removeFile(const std::string&amp; entity_name, const std::string&amp; filename)"

    Remove a file from an entity's directory.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name
        
    :material-location-enter: `filename`
    :    File name to remove
        
    :material-keyboard-return: **Return**
    :    true if file was successfully removed, false otherwise
    

### removeFiles<a name="removeFiles"></a>
!!! function "static void removeFiles(const std::string&amp; entity_name, const std::vector&lt;std::string&gt;&amp; files)"

    Remove multiple files from an entity's directory.
    
    
    :material-location-enter: `entity_name`
    :    Entity/table name
        
    :material-location-enter: `files`
    :    Vector of file names to remove
    

### renameDir<a name="renameDir"></a>
!!! function "static void renameDir(const std::string&amp; old_entity_name, const std::string&amp; new_entity_name)"

    Rename an entity directory.
    
    Renames the directory if it exists, otherwise creates a new one.
    Validates both old and new entity names.
    
    
    :material-location-enter: `old_entity_name`
    :    Current entity name
        
    :material-location-enter: `new_entity_name`
    :    New entity name
        @throws MantisException if either entity name is invalid
    

