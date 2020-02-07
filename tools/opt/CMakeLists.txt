set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  BitWriter
#  CodeGen # HLSL Change
  Core
  IPA
  IPO
  IRReader
  InstCombine
#  Instrumentation # HLSL Change
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
export_executable_symbols(opt)

# HLSL Change Starts
if (NOT HLSL_OPTIONAL_PROJS_IN_DEFAULT)
  set_target_properties(opt PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
endif ()
# HLSL Change Ends

if(WITH_POLLY AND LINK_POLLY_INTO_TOOLS)
  target_link_libraries(opt Polly)
  if(POLLY_LINK_LIBS)
    foreach(lib ${POLLY_LINK_LIBS})
      target_link_libraries(opt ${lib})
    endforeach(lib)
  endif(POLLY_LINK_LIBS)
endif(WITH_POLLY AND LINK_POLLY_INTO_TOOLS)
