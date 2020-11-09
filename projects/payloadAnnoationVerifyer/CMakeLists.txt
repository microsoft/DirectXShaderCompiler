# Builds payloadAccessVerifier.exe

macro(add_payload_project_executable name)
  add_llvm_executable(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES FOLDER "payloadAccessVerifier")
endmacro(add_payload_project_executable)

add_payload_project_executable(payloadAccessVerifier
  main.cpp
)

target_link_libraries(payloadAccessVerifier PRIVATE
  LLVMDXIL
  LLVMDxilContainer
  LLVMDxilRootSignature
  LLVMHLSL  
  LLVMMSSupport
)


include_directories()
