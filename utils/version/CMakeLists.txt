if(HLSL_EMBED_VERSION)
    string(REPLACE "/" "\\" HLSL_VERSION_LOCATION_mspath ${HLSL_VERSION_LOCATION})
    string(REPLACE "/" "\\" HLSL_FIXED_VERSION_LOCATION_mspath ${HLSL_FIXED_VERSION_LOCATION})

    set(HLSL_VERSION_FILE ${HLSL_VERSION_LOCATION_mspath}\\version.inc)
    if(HLSL_ENABLE_FIXED_VER)
        message("Using fixed version file ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc")
        add_custom_target(hlsl_version_autogen
            COMMAND echo "Using fixed version file ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc"
            COMMAND copy ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc ${HLSL_VERSION_LOCATION_mspath}
            )
    else(HLSL_ENABLE_FIXED_VER)
        message("Build will generate version from current date and time")
        add_custom_target(hlsl_version_autogen
            COMMAND echo "Generating version from current date and time"
            COMMAND py -3 ${CMAKE_CURRENT_SOURCE_DIR}/gen_version.py > ${HLSL_VERSION_FILE}
        )
        set_property(SOURCE ${HLSL_VERSION_FILE} 
            PROPERTY GENERATED True
        )
    endif(HLSL_ENABLE_FIXED_VER)
    
    set_target_properties(hlsl_version_autogen PROPERTIES
        FOLDER version
    )

endif(HLSL_EMBED_VERSION)