#include <gtest/gtest.h>
#include "mantisbase/core/files.h"
#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/utils/utils.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class FilesSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory
        test_base = fs::temp_directory_path() / "mantisbase_test_files" / mb::generateShortId();
        fs::create_directories(test_base);
        
        // Set up test entity name
        test_entity = "test_entity";
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_base)) {
            fs::remove_all(test_base);
        }
    }

    fs::path test_base;
    std::string test_entity;
};

TEST_F(FilesSecurityTest, EntityNameValidation) {
    // Valid entity names
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("valid_name"));
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("valid_name_123"));
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("a"));
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("a1b2c3"));

    // Invalid entity names
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("invalid-name"));  // Hyphen
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("invalid name"));  // Space
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("invalid.name")); // Dot
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName(""));              // Empty
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("name/with/slashes")); // Slashes
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("../parent"));     // Path traversal attempt
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("name_with_too_many_characters_abcdefghijklmnopqrstuvwxyz1234567890")); // Too long
}

TEST_F(FilesSecurityTest, FilenameSanitization) {
    // Test invalid characters are replaced
    std::string unsafe = "file*name?.txt";
    std::string safe = mb::sanitizeFilename(unsafe);
    
    EXPECT_NE(safe, unsafe);
    EXPECT_TRUE(safe.find('*') == std::string::npos);
    EXPECT_TRUE(safe.find('?') == std::string::npos);
    
    // Test that safe filenames remain mostly unchanged (except for unique ID suffix)
    std::string safe_input = "safe_filename.txt";
    std::string sanitized = mb::sanitizeFilename(safe_input);
    EXPECT_TRUE(sanitized.find("safe_filename") != std::string::npos);
}

TEST_F(FilesSecurityTest, PathTraversalPrevention) {
    // Create a test file structure
    auto entity_dir = test_base / "files" / test_entity;
    fs::create_directories(entity_dir);
    
    // Create a test file
    auto test_file = entity_dir / "test.txt";
    std::ofstream(test_file) << "test content";
    
    // Test that valid paths work
    auto valid_path = test_base / "files" / test_entity / "test.txt";
    EXPECT_TRUE(fs::exists(valid_path));
    
    // Test path traversal attempts
    // Note: getCanonicalPath and isCanonicalPath use filesBaseDir() which is based on MantisBase::instance().dataDir()
    // For unit tests, we need to test the logic differently
    
    // Test that "../" in entity name is rejected
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("../parent"));
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName("entity/../parent"));
    
    // Test that ".." in filename is handled by sanitizeFilename
    std::string traversal_filename = "../../etc/passwd";
    std::string sanitized = mb::sanitizeFilename(traversal_filename);
    EXPECT_NE(sanitized, traversal_filename);
    EXPECT_TRUE(sanitized.find("..") == std::string::npos);
}

TEST_F(FilesSecurityTest, InvalidCharDetection) {
    // Test invalid characters for filenames
    EXPECT_TRUE(mb::invalidChar('/'));
    EXPECT_TRUE(mb::invalidChar('\\'));
    EXPECT_TRUE(mb::invalidChar(':'));
    EXPECT_TRUE(mb::invalidChar('*'));
    EXPECT_TRUE(mb::invalidChar('?'));
    EXPECT_TRUE(mb::invalidChar('"'));
    EXPECT_TRUE(mb::invalidChar('<'));
    EXPECT_TRUE(mb::invalidChar('>'));
    EXPECT_TRUE(mb::invalidChar('|'));
    
    // Test valid characters
    EXPECT_FALSE(mb::invalidChar('a'));
    EXPECT_FALSE(mb::invalidChar('Z'));
    EXPECT_FALSE(mb::invalidChar('0'));
    EXPECT_FALSE(mb::invalidChar('9'));
    EXPECT_FALSE(mb::invalidChar('_'));
    EXPECT_FALSE(mb::invalidChar('-'));
    EXPECT_FALSE(mb::invalidChar('.'));
}

TEST_F(FilesSecurityTest, EntityNameLengthLimit) {
    // Test maximum length (64 characters)
    std::string max_length_name(64, 'a');
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName(max_length_name));
    
    // Test exceeding maximum length
    std::string too_long_name(65, 'a');
    EXPECT_FALSE(mb::EntitySchema::isValidEntityName(too_long_name));
}

TEST_F(FilesSecurityTest, EntityNameEdgeCases) {
    // Single character
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("a"));
    
    // Only numbers
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("123"));
    
    // Only underscores
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("___"));
    
    // Mixed valid characters
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("a1_b2_c3"));
    
    // Starting with number
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("1entity"));
    
    // Starting with underscore
    EXPECT_TRUE(mb::EntitySchema::isValidEntityName("_entity"));
}

TEST_F(FilesSecurityTest, FilenameSanitizationUniqueness) {
    // Test that sanitizeFilename adds unique IDs
    std::string filename = "test.txt";
    std::string sanitized1 = mb::sanitizeFilename(filename);
    std::string sanitized2 = mb::sanitizeFilename(filename);
    
    // Should be different due to unique ID
    EXPECT_NE(sanitized1, sanitized2);
    
    // But should contain the original name
    EXPECT_TRUE(sanitized1.find("test") != std::string::npos);
    EXPECT_TRUE(sanitized2.find("test") != std::string::npos);
}

TEST_F(FilesSecurityTest, FilenameLengthTruncation) {
    // Test that very long filenames are truncated
    std::string long_filename(200, 'a');
    long_filename += ".txt";
    
    std::string sanitized = mb::sanitizeFilename(long_filename, 50, 12);
    
    // Should be truncated (50 chars + separator + 12 char ID)
    EXPECT_LT(sanitized.length(), long_filename.length());
    EXPECT_GE(sanitized.length(), 50); // min length with ID
}

