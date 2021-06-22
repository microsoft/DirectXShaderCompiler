add_llvm_library(LLVMDxilResourceBinding
  DxilResourceBinding.cpp
)

add_dependencies(LLVMDxilResourceBinding intrinsics_gen)
