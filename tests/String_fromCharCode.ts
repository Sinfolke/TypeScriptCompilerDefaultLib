declare function _cchangeLocale(): void;
function main() {
    // Test 1: Basic characters
    _cchangeLocale();
    console.log(String.fromCharCode(65, 66, 67));  // Expected: "ABC"

    // Test 2: Single character
    console.log(String.fromCharCode(97));  // Expected: "a"

    // Test 3: Range of characters
    console.log(String.fromCharCode(48, 49, 50));  // Expected: "012"

    // Test 4: Non-BMP characters (Unicode)
    console.log(String.fromCharCode(0x1F600));  // Expected: "😀" (grinning face emoji)

    // Test 5: Empty string case (no characters)
    console.log(String.fromCharCode());  // Expected: "" (empty string)

    // Test 6: Characters with code points > 0xFFFF (surrogate pairs)
    console.log(String.fromCharCode(0xD83D, 0xDE00));  // Expected: "😀" (grinning face emoji)

    // Test 7: Invalid characters (large numbers that may not be valid Unicode)
    console.log(String.fromCharCode(70000));  // Expected: "\uD8F0" (Unicode replacement character)

    // Test 8: Combination of ASCII and Unicode characters
    console.log(String.fromCharCode(72, 101, 108, 108, 111, 0x1F60D));  // Expected: "Hello😍"

    // Test 9: Handling character with code 0 (null character)
    console.log(String.fromCharCode(0));  // Expected: "\0" (null character)
}