oso_add_test(
    NAME test_dom_common
    SOURCES test_dom_common.cpp
    DEPS oso_dom_common oso_ooxml_write oso_ooxml_read oso_io
)

oso_add_test(
    NAME test_dom_word
    SOURCES test_dom_word.cpp
    DEPS oso_dom_word oso_dom_common oso_ooxml_write oso_ooxml_read oso_io
)
