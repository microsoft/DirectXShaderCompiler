file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/version.gen" HLSL_GEN_VERSION_FILE)

# First create the DXC version header
# generated per gen_version.py except where explicit fixed version path is provided
if(HLSL_ENABLE_FIXED_VER AND "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")
    FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/version.inc" HLSL_FIXED_VERSION_FILE)
    message("Using fixed version file ${HLSL_FIXED_VERSION_FILE}")
    add_custom_command(
        OUTPUT ${HLSL_GEN_VERSION_FILE}
        COMMAND echo "Using fixed version file ${HLSL_FIXED_VERSION_FILE}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HLSL_FIXED_VERSION_FILE} ${HLSL_GEN_VERSION_FILE}
        DEPENDS ${HLSL_FIXED_VERSION_FILE}
    )
else(HLSL_ENABLE_FIXED_VER AND "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")
    # If official or the dxc case for fixed version location
    if (HLSL_OFFICIAL_BUILD OR HLSL_ENABLE_FIXED_VER)
        message("Will generate official build version based on the latest release fork sha and current commit count")
        set(HLSL_GEN_VERSION_OFFICIAL_OPTION "--official")
    else (HLSL_OFFICIAL_BUILD OR HLSL_ENABLE_FIXED_VER)
        message("Will generate dev build version based on current commit count")
        set(HLSL_GEN_VERSION_OFFICIAL_OPTION "")
    endif(HLSL_OFFICIAL_BUILD OR HLSL_ENABLE_FIXED_VER)

    add_custom_command(
        OUTPUT ${HLSL_GEN_VERSION_FILE}
        COMMAND echo "Generating version"
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_version.py ${HLSL_GEN_VERSION_OFFICIAL_OPTION} > ${HLSL_GEN_VERSION_FILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_version.py
    )
endif(HLSL_ENABLE_FIXED_VER AND "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")

# do dxcversion.inc copy
file(TO_NATIVE_PATH "${HLSL_VERSION_LOCATION}/dxcversion.inc" HLSL_DXC_VERSION_FILE)

add_custom_target(hlsl_dxcversion_autogen
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HLSL_GEN_VERSION_FILE} ${HLSL_DXC_VERSION_FILE}
    DEPENDS ${HLSL_GEN_VERSION_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_version.py
)

set_property(SOURCE ${HLSL_DXC_VERSION_FILE}
    PROPERTY GENERATED True
)

set_target_properties(hlsl_dxcversion_autogen PROPERTIES
    FOLDER version
)


# do version.inc copy
if (HLSL_EMBED_VERSION OR HLSL_ENABLE_FIXED_VER)
    # If there is an explicit fixed version location, version.inc should copy from there
    # Used to propagate GDK versions to the RC data
    if( NOT "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")
        FILE(TO_NATIVE_PATH "${HLSL_FIXED_VERSION_LOCATION}/version.inc" HLSL_COPY_VERSION_FILE)
    else( NOT "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")
        SET(HLSL_COPY_VERSION_FILE ${HLSL_GEN_VERSION_FILE})
    endif(NOT "${HLSL_FIXED_VERSION_LOCATION}" STREQUAL "")
    file(TO_NATIVE_PATH "${HLSL_VERSION_LOCATION}/version.inc" HLSL_VERSION_FILE)

    add_custom_target(hlsl_version_autogen
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HLSL_COPY_VERSION_FILE} ${HLSL_VERSION_FILE}
        DEPENDS ${HLSL_COPY_VERSION_FILE}
    )
    
    set_property(SOURCE ${HLSL_VERSION_FILE}
        PROPERTY GENERATED True
    )

    set_target_properties(hlsl_version_autogen PROPERTIES
        FOLDER version
    )

endif(HLSL_EMBED_VERSION OR HLSL_ENABLE_FIXED_VER)
