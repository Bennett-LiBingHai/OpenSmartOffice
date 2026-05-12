oso_add_module(
    NAME oso_core_ooxml
    DIR core/ooxml
    LAYER core
    PUBLIC_DEPS oso_base oso_ooxml_read oso_ooxml_write oso_dom_common oso_dom_word
)
