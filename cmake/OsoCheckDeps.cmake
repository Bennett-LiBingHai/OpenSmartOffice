#======================================================================
# OsoCheckDeps — 验证目标依赖关系，禁止跨层向上依赖
#
# 层级顺序 (从底层到顶层):
#   0: platform (最底层，基础类型/日志/IO/并发)
#   1: infra    (基础设施，OOXML 解析/公式/渲染)
#   2: core     (核心业务逻辑，DOM/Engine/Facade)
#   3: app      (应用层，CLI/GUI/Service)
#   tests: 特殊层，允许依赖任何层
#
# 同层依赖规则:
#   - Platform 层内: base 可被 logging/concurrent/io 依赖 (base 为基础类型库)
#   - 其他层: 同层模块之间禁止直接依赖
#   - 跨层: 下层禁止依赖上层
#
# 调用时机: 在顶级 CMakeLists.txt 的末尾调用
#======================================================================
function(oso_check_deps)
    #------------------------------------------------------------------
    # 1. 定义层级顺序 (仅用于记录，实际映射在 _oso_layer_rank 中)
    #------------------------------------------------------------------
    set(_layer_order platform infra core app)

    #------------------------------------------------------------------
    # 2. 收集项目中所有的 OSO 库目标
    #------------------------------------------------------------------
    set(_targets "")
    _oso_collect_oso_targets(_targets) # 调用辅助函数填充 _targets

    #------------------------------------------------------------------
    # 3. 遍历每个目标，检查其依赖
    #------------------------------------------------------------------
    foreach(_tgt ${_targets})
        # 获取当前目标所属的层级 (读取之前由 oso_add_module 设置的属性)
        get_target_property(_layer ${_tgt} OSO_LAYER)
        
        # 如果目标没有设置层级属性 (非 OSO 标准模块)，跳过检查
        if(NOT _layer)
            continue()
        endif()

        # 将当前层级名称转换为数字 (如 "platform" -> 0)
        _oso_layer_rank(${_layer} _this_rank)

        #------------------------------------------------------------------
        # 4. 获取当前目标的所有链接依赖
        #------------------------------------------------------------------
        # LINK_LIBRARIES 包含了 PUBLIC 和 PRIVATE 链接的所有库
        get_target_property(_deps ${_tgt} LINK_LIBRARIES)
        
        # 如果没有依赖，直接跳过
        if(NOT _deps)
            continue()
        endif()

        #------------------------------------------------------------------
        # 5. 遍历每一个依赖，进行层级校验
        #------------------------------------------------------------------
        foreach(_dep ${_deps})
            # 检查依赖项是否是一个 CMake 目标 (排除系统库，如 -lpthread)
            if(NOT TARGET ${_dep})
                continue()
            endif()

            # 获取依赖项的层级
            get_target_property(_dep_layer ${_dep} OSO_LAYER)
            
            # 如果依赖项没有层级属性 (如 Qt6::Core)，跳过
            if(NOT _dep_layer)
                continue()
            endif()

            # 将依赖项的层级名称转换为数字
            _oso_layer_rank(${_dep_layer} _dep_rank)

            #------------------------------------------------------------------
            # 6. 核心规则判定：下层不能依赖上层
            #------------------------------------------------------------------
            # 如果依赖项的层级数字 > 当前目标的层级数字
            # (例如: 当前是 platform(0)，依赖了 core(2) -> 违规)
            if(_dep_rank GREATER _this_rank)
                message(FATAL_ERROR
                    "Layer violation: ${_tgt} (${_layer}) depends on ${_dep} (${_dep_layer}). "
                    "Lower layers must not depend on upper layers."
                )
            endif()

            # 同层依赖：仅 Platform 层内允许 base 被同层依赖
            if(_dep_rank EQUAL _this_rank AND NOT _layer STREQUAL "platform")
                message(FATAL_ERROR
                    "Layer violation: ${_tgt} (${_layer}) depends on ${_dep} (${_dep_layer}). "
                    "Same-layer dependencies are forbidden outside the Platform layer."
                )
            endif()
        endforeach()
    endforeach()

    # 所有检查通过，打印成功信息
    message(STATUS "Layer dependency check: PASSED")
endfunction()

#======================================================================
# 辅助函数 1: 收集所有 OSO 目标
# 输入: _out (输出变量名)
#======================================================================
function(_oso_collect_oso_targets _out)
    set(_result "")
    # 从源码根目录开始递归收集
    _oso_collect_recursive("${CMAKE_SOURCE_DIR}" _result)
    # 将结果列表传递给父作用域
    set(${_out} ${_result} PARENT_SCOPE)
endfunction()

#======================================================================
# 辅助函数 2: 递归遍历目录收集目标
# 输入: _dir (当前目录), _out (结果列表变量名)
#======================================================================
function(_oso_collect_recursive _dir _out)
    # 1. 继承上一层的结果列表 (因为 function 有自己的作用域)
    set(_result ${${_out}})

    # 2. 获取当前目录的属性
    # 获取子目录列表
    get_directory_property(_subdirs DIRECTORY ${_dir} SUBDIRECTORIES)
    # 获取当前目录下定义的所有构建目标 (Target)
    get_directory_property(_tgts DIRECTORY ${_dir} BUILDSYSTEM_TARGETS)

    # 3. 筛选出以 "oso_" 开头的目标 (我们的库)
    foreach(_tgt ${_tgts})
        if(_tgt MATCHES "^oso_")
            list(APPEND _result ${_tgt})
        endif()
    endforeach()

    # 4. 递归处理所有子目录
    foreach(_sub ${_subdirs})
        _oso_collect_recursive(${_sub} _result)
    endforeach()

    # 5. 将更新后的结果列表传递给父作用域
    set(${_out} ${_result} PARENT_SCOPE)
endfunction()

#======================================================================
# 辅助函数 3: 将层级名称映射为数字 (Rank)
# 输入: _layer (层级名), _out (输出变量名)
#======================================================================
function(_oso_layer_rank _layer _out)
    if(_layer STREQUAL "platform")
        set(${_out} 0 PARENT_SCOPE)   # 最底层
    elseif(_layer STREQUAL "infra")
        set(${_out} 1 PARENT_SCOPE)
    elseif(_layer STREQUAL "core")
        set(${_out} 2 PARENT_SCOPE)
    elseif(_layer STREQUAL "app")
        set(${_out} 3 PARENT_SCOPE)   # 最顶层
    elseif(_layer STREQUAL "tests")
        set(${_out} 99 PARENT_SCOPE)  # 测试层层级最高，可依赖任意层
    else()
        # 遇到未知层级，直接报错终止构建
        message(FATAL_ERROR "Unknown layer '${_layer}' for target. "
                            "Valid layers: platform, infra, core, app, tests")
    endif()
endfunction()