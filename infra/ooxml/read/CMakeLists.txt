oso_add_module(
    NAME oso_ooxml_read
    DIR infra/ooxml/read
    LAYER infra
    PUBLIC_DEPS oso_base oso_io oso_ooxml_common
    PRIVATE_DEPS LibXml2::LibXml2
)
