#include <gtest/gtest.h>
#include "mantisbase/utils/utils.h"

TEST(IPValidation, ValidIPv4) {
    // Valid IPv4 addresses
    EXPECT_TRUE(mb::isValidIPv4("192.168.1.1"));
    EXPECT_TRUE(mb::isValidIPv4("10.0.0.1"));
    EXPECT_TRUE(mb::isValidIPv4("127.0.0.1"));
    EXPECT_TRUE(mb::isValidIPv4("0.0.0.0"));
    EXPECT_TRUE(mb::isValidIPv4("255.255.255.255"));
    EXPECT_TRUE(mb::isValidIPv4("1.1.1.1"));
    EXPECT_TRUE(mb::isValidIPv4("172.16.0.1"));
}

TEST(IPValidation, InvalidIPv4) {
    // Invalid IPv4 addresses
    EXPECT_FALSE(mb::isValidIPv4(""));                    // Empty
    EXPECT_FALSE(mb::isValidIPv4("256.1.1.1"));           // Out of range
    EXPECT_FALSE(mb::isValidIPv4("192.168.1"));           // Missing octet
    EXPECT_FALSE(mb::isValidIPv4("192.168.1.1.1"));       // Too many octets
    EXPECT_FALSE(mb::isValidIPv4("192.168.1."));          // Trailing dot
    EXPECT_FALSE(mb::isValidIPv4(".192.168.1.1"));        // Leading dot
    EXPECT_FALSE(mb::isValidIPv4("192.168.1.1 "));        // Trailing space
    EXPECT_FALSE(mb::isValidIPv4(" 192.168.1.1"));        // Leading space
    EXPECT_FALSE(mb::isValidIPv4("192.168.1.-1"));        // Negative number
    EXPECT_FALSE(mb::isValidIPv4("192.168.1.abc"));       // Non-numeric
    EXPECT_FALSE(mb::isValidIPv4("192.168.1"));          // Incomplete
}

TEST(IPValidation, ValidIPv6) {
    // Valid IPv6 addresses (common formats)
    EXPECT_TRUE(mb::isValidIPv6("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    EXPECT_TRUE(mb::isValidIPv6("2001:db8:85a3::8a2e:370:7334"));  // Compressed
    EXPECT_TRUE(mb::isValidIPv6("::1"));                            // Localhost
    EXPECT_TRUE(mb::isValidIPv6("::"));                             // All zeros
    EXPECT_TRUE(mb::isValidIPv6("2001:db8::1"));                    // Compressed
    EXPECT_TRUE(mb::isValidIPv6("fe80::1"));                        // Link-local
}

TEST(IPValidation, InvalidIPv6) {
    // Invalid IPv6 addresses
    EXPECT_FALSE(mb::isValidIPv6(""));                              // Empty
    EXPECT_FALSE(mb::isValidIPv6("2001:0db8:85a3::8a2e:0370:7334:1234:5678")); // Too many segments
    EXPECT_FALSE(mb::isValidIPv6("2001::db8::85a3"));               // Multiple ::
    EXPECT_FALSE(mb::isValidIPv6("2001:db8:85a3:8a2e:370:7334"));  // Too few segments
    EXPECT_FALSE(mb::isValidIPv6("2001:0db8:85a3:0000:0000:8a2e:0370:7334:1234")); // Too many
    EXPECT_FALSE(mb::isValidIPv6("gggg::1"));                      // Invalid hex
    EXPECT_FALSE(mb::isValidIPv6("2001:db8:85a3::8a2e:370:7334 ")); // Trailing space
}

TEST(IPValidation, ValidIP) {
    // Should accept both IPv4 and IPv6
    EXPECT_TRUE(mb::isValidIP("192.168.1.1"));
    EXPECT_TRUE(mb::isValidIP("2001:db8::1"));
    EXPECT_TRUE(mb::isValidIP("127.0.0.1"));
    EXPECT_TRUE(mb::isValidIP("::1"));
}

TEST(IPValidation, InvalidIP) {
    // Should reject invalid formats
    EXPECT_FALSE(mb::isValidIP(""));
    EXPECT_FALSE(mb::isValidIP("not.an.ip"));
    EXPECT_FALSE(mb::isValidIP("192.168.1"));
    EXPECT_FALSE(mb::isValidIP("localhost"));
    EXPECT_FALSE(mb::isValidIP("example.com"));
}

TEST(IPValidation, EdgeCases) {
    // Boundary values for IPv4
    EXPECT_TRUE(mb::isValidIPv4("0.0.0.0"));        // Minimum
    EXPECT_TRUE(mb::isValidIPv4("255.255.255.255")); // Maximum
    EXPECT_FALSE(mb::isValidIPv4("256.0.0.0"));     // Out of range
    EXPECT_FALSE(mb::isValidIPv4("0.256.0.0"));     // Out of range
    
    // Length limits
    EXPECT_FALSE(mb::isValidIPv4("1.2.3.4.5"));     // Too long
    EXPECT_FALSE(mb::isValidIPv4("1.2.3"));         // Too short
}

TEST(IPValidation, IPv6CompressedNotation) {
    // Test various compressed notation forms
    EXPECT_TRUE(mb::isValidIPv6("2001:db8::1"));           // Single compression
    EXPECT_TRUE(mb::isValidIPv6("::ffff:192.168.1.1"));    // IPv4-mapped
    EXPECT_TRUE(mb::isValidIPv6("2001:db8:85a3::8a2e:370:7334")); // Middle compression
}

