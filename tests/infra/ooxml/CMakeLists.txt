set(TEST_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

oso_add_test(
    NAME test_ooxml_common
    SOURCES test_ooxml_common.cpp
    DEPS oso_ooxml_common
)

# Pass fixture directory to test
target_compile_definitions(test_ooxml_common PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)

oso_add_test(
    NAME test_ooxml_read
    SOURCES test_ooxml_read.cpp
    DEPS oso_ooxml_read oso_ooxml_common
)

target_compile_definitions(test_ooxml_read PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)

oso_add_test(
    NAME test_ooxml_write
    SOURCES test_ooxml_write.cpp
    DEPS oso_ooxml_write oso_ooxml_read oso_ooxml_common
)

target_compile_definitions(test_ooxml_write PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)
