# func:
#     1. Generate kbuild file
#     2. Create target and add to all, to execute make command
# usage:
#    ADD_KBUILD(m_kDemo SRC kDemo.c DEPS <dep1> [<dep2>])
#    depn is ko target
FUNCTION(ADD_KBUILD MODULE_NAME)
    SET(args ${ARGN})
    SET(add_src 0)
    SET(add_dep 0)

    SET(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    SET(MODULE_KO "${MODULE_NAME}.ko")
    SET(MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build")
    SET(MODULE_SRCS "")
    SET(MODULE_DEPS "")
    SET(MODULE_OBJS "")
    SET(MODULE_CFLAG "")
    SET(MODULE_EXTRA_SYM "")
    SET(MODULE_BYPRODUCTS "")
    SET(MODULE_OBJS_INFO "")

    FOREACH(arg IN LISTS args)
        IF(arg STREQUAL "SRC")
            SET(add_src 1)
            SET(add_dep 0)
        ELSEIF(arg STREQUAL "DEPS")
            SET(add_src 0)
            SET(add_dep 1)
        ELSE()
            IF(add_src)
                LIST(APPEND MODULE_SRCS "${arg}")
            ELSEIF(add_dep)
                LIST(APPEND MODULE_DEPS "${arg}")
            ENDIF()
        ENDIF()
    ENDFOREACH()

    STRING(STRIP "${MODULE_SRCS}" MODULE_SRCS)
    STRING(STRIP "${MODULE_DEPS}" MODULE_DEPS)

    FILE(MAKE_DIRECTORY ${MODULE_DIR})

    # proc CFLAG
    GET_DIRECTORY_PROPERTY(DIR_DEFS COMPILE_DEFINITIONS)
    MESSAGE(STATUS "CMP DIR_DEFS: ${DIR_DEFS}")
    FOREACH(d ${DIR_DEFS})
        SET(MODULE_CFLAG "${MODULE_CFLAG} -D${d}")
    ENDFOREACH()
    MESSAGE(STATUS "MODULE_CFLAG: ${MODULE_CFLAG}")

    GET_DIRECTORY_PROPERTY(DIR_DEFS INCLUDE_DIRECTORIES )
    MESSAGE(STATUS "INC DIR_DEFS: ${DIR_DEFS}")
    FOREACH(d ${DIR_DEFS})
        SET(MODULE_CFLAG "${MODULE_CFLAG} -I${d}")
    ENDFOREACH()
    MESSAGE(STATUS "MODULE_CFLAG: ${MODULE_CFLAG}")

    # IF(${ARCH_TYPE} STREQUAL "arm64")
    #     SET(MODULE_CFLAG "${MODULE_CFLAG} -mno-outline-atomics ")
    # ENDIF()

    # proc MODULE_OBJS
    FOREACH(src ${MODULE_SRCS})
        IF(NOT IS_ABSOLUTE ${src})
            SET(src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        ENDIF()

        GET_FILENAME_COMPONENT(name ${src} NAME)

        FILE(RELATIVE_PATH dst_rel_path ${SOURCE_DIR} ${src})
        GET_FILENAME_COMPONENT(rel_dir ${dst_rel_path} DIRECTORY)

        IF(NOT EXISTS "${MODULE_DIR}/${rel_dir}")
            FILE(MAKE_DIRECTORY ${MODULE_DIR}/${rel_dir})
        ENDIF()

        SET(dst "${MODULE_DIR}/${dst_rel_path}")

        EXECUTE_PROCESS(COMMAND bash -c "ln -sf ${src} ${dst}")

        # NAME_WE 中 WE 代表 “Without Extension”，即“无扩展名”
        GET_FILENAME_COMPONENT(name_we ${dst} NAME_WE)
        SET(obj "./${rel_dir}/${name_we}.o")

        STRING(APPEND MODULE_OBJS "${obj} ")

        LIST(APPEND MODULE_BYPRODUCTS
            "${MODULE_DIR}/${obj}"
            "${MODULE_DIR}/${obj}.cmd"
        )
    ENDFOREACH()
    MESSAGE(STATUS "MODULE_OBJS: ${MODULE_OBJS}")

    LIST(LENGTH MODULE_SRCS OBJS_FILE_COUNT)
    IF(${OBJS_FILE_COUNT} GREATER 1)
        SET(MODULE_OBJS_INFO "${MODULE_NAME}-objs := ${MODULE_OBJS}")
    ENDIF()

    FOREACH(d ${MODULE_DEPS})
        GET_PROPERTY(dep_dir TARGET ${d} PROPERTY KO_LOCATION)
        STRING(APPEND MODULE_EXTRA_SYM "${dep_dir}/Module.symvers ")
    ENDFOREACH()

    # MODULE_BYPRODUCTS
    LIST(APPEND MODULE_BYPRODUCTS
        "${MODULE_DIR}/.${MODULE_KO}.cmd"
        "${MODULE_DIR}/.${MODULE_NAME}.mod.o.cmd"
        "${MODULE_DIR}/.${MODULE_NAME}.o.cmd"
        "${MODULE_DIR}/.built-in.o.cmd"
        "${MODULE_DIR}/${MODULE_NAME}.mod.c"
        "${MODULE_DIR}/${MODULE_NAME}.mod.o"
        "${MODULE_DIR}/${MODULE_NAME}.o"
        "${MODULE_DIR}/built-in.o"
        "${MODULE_DIR}/Module.symvers"
        "${MODULE_DIR}/modules.order"
    )

    MESSAGE(STATUS "MODULE_BYPRODUCTS: ${MODULE_BYPRODUCTS}")
    MESSAGE(STATUS "KERNEL_MAKE_OPT: ${KERNEL_MAKE_OPT}")

    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/Kbuild.in ${MODULE_DIR}/Kbuild @ONLY)

    SET(KBUILD_CMD
        PATH=$ENV{PATH}
        ${CMAKE_MAKE_PROGRAM}
        modules
        V=1
        -C ${KERNEL_DIR}
        # ARCH=${ARCH_TYPE}
        # CROSS_COMPILE=${CROSS_COMPILER_PREFIX}
        ${KERNEL_MAKE_OPT}
        M=${MODULE_DIR}
        src=${MODULE_DIR}
    )
    MESSAGE(STATUS "KBUILD_CMD: ${KBUILD_CMD}")

    # BYPRODUCTS 参数告诉 CMake，自定义命令会生成哪些额外的文件，这样 CMake 可以
    # 追踪这些文件的状态，确保它们是最新的。这对于构建系统的依赖关系管理非常重要，
    # 因为 CMake 需要知道何时需要重新执行自定义命令
    ADD_CUSTOM_COMMAND(
        OUTPUT ${MODULE_DIR}/${MODULE_KO}
        COMMAND ${KBUILD_CMD}
        DEPENDS ${MODULE_SRCS}
        WORKING_DIRECTORY ${MODULE_DIR}
        BYPRODUCTS ${MODULE_BYPRODUCTS}
        COMMENT "Building ${MODULE_KO}"
        VERBATIM
    )

    ADD_CUSTOM_TARGET(
        ${MODULE_NAME} ALL
        COMMAND ${KBUILD_CMD}
        DEPENDS ${MODULE_DIR}/${MODULE_KO}
        DEPENDS ${MODULE_DEPS}
    )

    # clean for others module
    SET_TARGET_PROPERTIES(
        ${MODULE_NAME}
        PROPERTIES
        OUTPUT_NAME         ${MODULE_NAME}
        KO_LOCATION         ${MODULE_DIR}
        MODULE_KO           ""
        MODULE_DIR          ""
        MODULE_SRCS         ""
        MODULE_DEPS         ""
        MODULE_OBJS         ""
        MODULE_CFLAG        ""
        MODULE_EXTRA_SYM    ""
        MODULE_BYPRODUCTS   ""
        )
ENDFUNCTION()
