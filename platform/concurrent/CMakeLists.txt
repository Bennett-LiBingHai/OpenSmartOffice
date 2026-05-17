oso_add_module(
    NAME oso_concurrent
    DIR platform/concurrent
    LAYER platform
    PUBLIC_DEPS  oso_base oso_logging
    PRIVATE_DEPS Qt6::Core Qt6::Concurrent
)
