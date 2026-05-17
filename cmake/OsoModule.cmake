# 全局严格警告函数
function(oso_add_strict_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wnon-virtual-dtor
        >-Wall -Wextra -Wpedantic -Werror
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
            /WX
        >
    )
endfunction()

#======================================================================
#  oso_add_module — 用于添加 OSO 库目标的标准宏
#
#  使用示例:
#    oso_add_module(
#        NAME oso_base          # 库目标名称,必填
#        DIR platform/base      # 库在源码树中的相对路径,必填
#        LAYER platform         # 所属架构层 (用于依赖校验)
#        PUBLIC_DEPS Qt6::Core  # 公开依赖 (传递给链接者)
#        PRIVATE_DEPS spdlog    # 私有依赖 (仅内部使用)
#    )
#
#  [LAYER] 参数会被记录为目标属性，用于后续依赖关系验证
#======================================================================
macro(oso_add_module)
    #------------------------------------------------------------------
    # 1. 参数解析 (使用 CMake 内置的 cmake_parse_arguments)
    #------------------------------------------------------------------
    # 参数签名说明:
    # - 前缀: OSO_MODULE (所有解析出的变量都以此为前缀)
    # - 选项参数: (无，此处为空字符串)
    # - 单值参数: NAME, DIR, LAYER
    # - 多值参数: PUBLIC_DEPS, PRIVATE_DEPS, SOURCES
    # - 输入: ${ARGN} (宏调用时的所有参数列表)
    cmake_parse_arguments(
        OSO_MODULE
        "NO_STRICT_WARNINGS"
        "NAME;DIR;LAYER"
        "PUBLIC_DEPS;PRIVATE_DEPS;SOURCES;COMPILE_OPTIONS"
        ${ARGN}
    )

    #------------------------------------------------------------------
    # 2. 必需参数校验
    #------------------------------------------------------------------
    # 检查是否提供了 NAME (库目标名称)
    if(NOT OSO_MODULE_NAME)
        message(FATAL_ERROR "oso_add_module: NAME is required")
    endif()
    # 检查是否提供了 DIR (源码目录路径)
    if(NOT OSO_MODULE_DIR)
        message(FATAL_ERROR "oso_add_module: DIR is required")
    endif()

    #------------------------------------------------------------------
    # 3. 源文件收集逻辑
    #------------------------------------------------------------------
    # 初始化源文件列表为空
    set(_sources "")
    
    # 如果用户显式指定了 SOURCES，则添加到列表
    if(OSO_MODULE_SOURCES)
        set(_sources ${OSO_MODULE_SOURCES})
    endif()

    # 自动收集逻辑: 检查 <DIR>/src/ 目录是否存在
    if(EXISTS "${CMAKE_SOURCE_DIR}/${OSO_MODULE_DIR}/src")
        # 递归匹配所有 .cpp 和 .cc 文件
        # GLOB_RECURSE 会遍历子目录
        file(GLOB_RECURSE _auto_sources
            "${CMAKE_SOURCE_DIR}/${OSO_MODULE_DIR}/src/*.cpp"
            "${CMAKE_SOURCE_DIR}/${OSO_MODULE_DIR}/src/*.cc"
        )
        # 将自动收集的文件追加到源文件列表
        list(APPEND _sources ${_auto_sources})
    endif()

    #------------------------------------------------------------------
    # 4. 创建库目标 (静态库 vs 头文件-only 库)
    #------------------------------------------------------------------
    if(_sources)
        # 情况 A: 有源文件 -> 创建 STATIC (静态) 库
        add_library(${OSO_MODULE_NAME} STATIC ${_sources})
    else()
        # 情况 B: 无源码 -> 创建 INTERFACE (头文件-only) 库
        # 这种库不编译，仅用于传递头文件和依赖
        add_library(${OSO_MODULE_NAME} INTERFACE)
    endif()

    #------------------------------------------------------------------
    # 5. 配置头文件包含路径
    #------------------------------------------------------------------
    # 设定标准的 include 路径: <DIR>/include/
    set(_include_dir "${CMAKE_SOURCE_DIR}/${OSO_MODULE_DIR}/include")
    
    # INTERFACE 库只能用 INTERFACE 关键字，STATIC 库用 PUBLIC/PRIVATE
    if(_sources)
        set(_scope PUBLIC)
    else()
        set(_scope INTERFACE)
    endif()

    # 如果该目录存在，则配置到目标中
    if(EXISTS "${_include_dir}")
        target_include_directories(${OSO_MODULE_NAME}
            ${_scope}
                # $<BUILD_INTERFACE>: 仅在构建时生效的路径
                $<BUILD_INTERFACE:${_include_dir}>
                # $<INSTALL_INTERFACE>: 仅在安装(make install)时生效的路径
                $<INSTALL_INTERFACE:include>
        )
    endif()

    #------------------------------------------------------------------
    # 6. 配置依赖链接
    #------------------------------------------------------------------
    # 处理公开依赖 (依赖会传递给链接此库的其他目标)
    if(OSO_MODULE_PUBLIC_DEPS)
        target_link_libraries(${OSO_MODULE_NAME} ${_scope} ${OSO_MODULE_PUBLIC_DEPS})
    endif()

    # 处理私有依赖 (依赖仅内部使用，不传递)
    # 仅 STATIC 库支持 PRIVATE；INTERFACE 库统一用 INTERFACE
    if(OSO_MODULE_PRIVATE_DEPS)
        if(_sources)
            target_link_libraries(${OSO_MODULE_NAME} PRIVATE ${OSO_MODULE_PRIVATE_DEPS})
        else()
            target_link_libraries(${OSO_MODULE_NAME} INTERFACE ${OSO_MODULE_PRIVATE_DEPS})
        endif()
    endif()

    #------------------------------------------------------------------
    # 7. 设置架构层元数据 (自定义属性)
    #------------------------------------------------------------------
    # 将 LAYER 参数保存为目标属性 "OSO_LAYER"
    # 后续可通过 get_target_property 读取，用于架构分层校验
    if(OSO_MODULE_LAYER)
        set_target_properties(${OSO_MODULE_NAME} PROPERTIES
            OSO_LAYER "${OSO_MODULE_LAYER}"
        )
    endif()

    #------------------------------------------------------------------
    # 8. 配置编译选项和严格警告
    #------------------------------------------------------------------
    # 为静态库默认添加严格警告，除非显式指定NO_STRICT_WARNINGS
    if(_sources AND NOT OSO_MODULE_NO_STRICT_WARNINGS)
        oso_add_strict_warnings(${OSO_MODULE_NAME})
    endif()

    # 添加模块特定的编译选项
    if(OSO_MODULE_COMPILE_OPTIONS)
        if(_sources)
            target_compile_options(${OSO_MODULE_NAME} PRIVATE
                ${OSO_MODULE_COMPILE_OPTIONS}
            )
        else()
            target_compile_options(${OSO_MODULE_NAME} INTERFACE
                ${OSO_MODULE_COMPILE_OPTIONS}
            )
        endif()
    endif()

    #------------------------------------------------------------------
    # 9. 创建命名空间别名 (Alias)
    #------------------------------------------------------------------
    # 逻辑: 将 "oso_base" 替换为 "oso::base"
    string(REPLACE "oso_" "oso::" _alias ${OSO_MODULE_NAME})
    
    add_library(${_alias} ALIAS ${OSO_MODULE_NAME})
endmacro()

#======================================================================
#  oso_add_test — 用于添加测试可执行文件的标准宏
#
#  使用示例:
#    oso_add_test(
#        NAME test_base          # 测试可执行文件名,必填
#        SOURCES test_base.cpp   # 测试源文件,必填
#        DEPS oso_base           # 被测模块依赖
#    )
#
#  自动链接 GTest::gtest_main，设置 OSO_LAYER=tests，注册 CTest。
#======================================================================
macro(oso_add_test)
    cmake_parse_arguments(
        OSO_TEST
        ""
        "NAME"
        "SOURCES;DEPS"
        ${ARGN}
    )

    if(NOT OSO_TEST_NAME)
        message(FATAL_ERROR "oso_add_test: NAME is required")
    endif()
    if(NOT OSO_TEST_SOURCES)
        message(FATAL_ERROR "oso_add_test: SOURCES is required")
    endif()

    add_executable(${OSO_TEST_NAME} ${OSO_TEST_SOURCES})
    target_link_libraries(${OSO_TEST_NAME} PRIVATE
        ${OSO_TEST_DEPS}
        GTest::gtest_main
    )
    set_target_properties(${OSO_TEST_NAME} PROPERTIES
        OSO_LAYER "tests"
    )
    add_test(NAME ${OSO_TEST_NAME} COMMAND ${OSO_TEST_NAME})
endmacro()