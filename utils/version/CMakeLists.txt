if(HLSL_EMBED_VERSION)
    string(REPLACE "/" "\\" HLSL_VERSION_LOCATION_mspath ${HLSL_VERSION_LOCATION})

    set(HLSL_VERSION_FILE ${HLSL_VERSION_LOCATION_mspath}\\version.inc)
    if(HLSL_ENABLE_FIXED_VER)
        string(REPLACE "/" "\\" HLSL_FIXED_VERSION_LOCATION_mspath ${HLSL_FIXED_VERSION_LOCATION})
        message("Using fixed version file ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc")
        add_custom_target(hlsl_version_autogen
            COMMAND echo "Using fixed version file ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc"
            COMMAND copy ${HLSL_FIXED_VERSION_LOCATION_mspath}\\version.inc ${HLSL_VERSION_LOCATION_mspath}
            )
    else(HLSL_ENABLE_FIXED_VER)
        if (HLSL_OFFICIAL_BUILD)
            message("Will generate official build version based on the latest release fork sha and current commit count")
            set(GEN_VERSION_OFFICIAL_OPTION "--official")
        else (HLSL_OFFICIAL_BUILD)
            message("Will generate dev build version based on current commit count")
            set(GEN_VERSION_OFFICIAL_OPTION "")
        endif(HLSL_OFFICIAL_BUILD)

        add_custom_target(hlsl_version_autogen
            COMMAND echo "Generating version"
            COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_version.py ${GEN_VERSION_OFFICIAL_OPTION} > ${HLSL_VERSION_FILE}
        )
        set_property(SOURCE ${HLSL_VERSION_FILE} 
            PROPERTY GENERATED True
        )
    endif(HLSL_ENABLE_FIXED_VER)
    
    set_target_properties(hlsl_version_autogen PROPERTIES
        FOLDER version
    )

endif(HLSL_EMBED_VERSION)