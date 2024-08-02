set(LLVM_LINK_COMPONENTS
  BitWriter
  Core
  ScalarOpts
  IRReader
  Support
  )

add_llvm_tool(llvm-extract
  llvm-extract.cpp
  )
