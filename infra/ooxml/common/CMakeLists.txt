oso_add_module(
    NAME oso_ooxml_common
    DIR infra/ooxml/common
    LAYER infra
    PUBLIC_DEPS oso_base oso_logging
    PRIVATE_DEPS libzip::zip LibXml2::LibXml2
)
