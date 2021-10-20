set(LLVM_LINK_COMPONENTS
  BitWriter
  Core
  DXIL
  IPO
  IRReader
  Support
  )

add_llvm_tool(llvm-extract
  llvm-extract.cpp
  )
