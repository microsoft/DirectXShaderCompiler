set(LLVM_LINK_COMPONENTS
  Support
  dxcSupport
  )

# Need to add the dir where the test dxil lib ends up to the path
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path;${CMAKE_INSTALL_RPATH}")
else(UNIX)
  set(CMAKE_INSTALL_RPATH "\$ORIGIN;${CMAKE_INSTALL_RPATH}")
endif()

add_llvm_unittest(DxilLibTests
  DxilLibTest.cpp
  )

add_dependencies(DxilLibTests dxil dxcompiler)


if(WIN32)
  set(SUBDIR, $<CONFIGURATION>)
else(WIN32)
  set(SUBDIR, '.')
endif(WIN32)

add_custom_command(TARGET DxilLibTests POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dxil> ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}dxil${CMAKE_SHARED_LIBRARY_SUFFIX}
   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dxcompiler> ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}dxcompiler${CMAKE_SHARED_LIBRARY_SUFFIX}
)

