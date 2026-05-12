add_subdirectory(dom)

set(TEST_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../infra/ooxml/data")

oso_add_test(
    NAME test_core_ooxml
    SOURCES test_core_ooxml.cpp
    DEPS oso_core_facade oso_core_ooxml oso_ooxml_common oso_ooxml_read oso_ooxml_write
)

target_compile_definitions(test_core_ooxml PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)

oso_add_test(
    NAME test_core_facade
    SOURCES test_core_facade.cpp
    DEPS oso_core_facade oso_core_ooxml oso_ooxml_common oso_ooxml_read oso_ooxml_write
)

target_compile_definitions(test_core_facade PRIVATE
    TEST_FIXTURE_DIR="${TEST_FIXTURE_DIR}"
)
