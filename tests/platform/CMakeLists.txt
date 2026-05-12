# M1 platform tests

oso_add_test(
    NAME test_base
    SOURCES test_base.cpp
    DEPS oso_base
)

oso_add_test(
    NAME test_io
    SOURCES test_io.cpp
    DEPS oso_io
)

oso_add_test(
    NAME test_logging
    SOURCES test_logging.cpp
    DEPS oso_logging
)

oso_add_test(
    NAME test_concurrent
    SOURCES test_concurrent.cpp
    DEPS oso_concurrent Qt6::Core Qt6::Concurrent
)

