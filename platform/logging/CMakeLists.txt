oso_add_module(
    NAME oso_logging
    DIR platform/logging
    LAYER platform
    PRIVATE_DEPS spdlog::spdlog
)
