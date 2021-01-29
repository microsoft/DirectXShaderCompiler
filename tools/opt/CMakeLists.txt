set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  BitWriter
#  CodeGen # HLSL Change
  Core
  DXIL # HLSL Change
  HLSL # HLSL Change
  IPA
  IPO
  IRReader
  InstCombine
#  Instrumentation # HLSL Change
  Passes # HLSL Change
#  MC # HLSL Change
#  ObjCARCOpts # HLSL Change
  Passes
  PassPrinters # HLSL Change
  ScalarOpts
  Support
  Target
  TransformUtils
  Vectorize
)

# Support plugins.
set(LLVM_NO_DEAD_STRIP 1)

set(LLVM_LINK_COMPONENTS ${LLVM_LINK_COMPONENTS} mssupport hlsl)  # HLSL Change

add_llvm_tool(opt
  AnalysisWrappers.cpp
  BreakpointPrinter.cpp
  GraphPrinters.cpp
  NewPMDriver.cpp
  PrintSCC.cpp
  opt.cpp
)

if(WIN32)
target_link_libraries(opt
   DxilConvPasses
)
add_dependencies(opt DxilConvPasses)
endif(WIN32)

export_executable_symbols(opt)

if(WITH_POLLY AND LINK_POLLY_INTO_TOOLS)
  target_link_libraries(opt Polly)
  if(POLLY_LINK_LIBS)
    foreach(lib ${POLLY_LINK_LIBS})
      target_link_libraries(opt ${lib})
    endforeach(lib)
  endif(POLLY_LINK_LIBS)
endif(WITH_POLLY AND LINK_POLLY_INTO_TOOLS)
