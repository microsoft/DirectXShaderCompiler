///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// DxilModuleTest.cpp                                                        //
//                                                                           //
// Provides unit tests for DxilModule.                                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "CompilationResult.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/HLSL/HLOperationLowerExtension.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilModule.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"

using namespace hlsl;
using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// DxilModule unit tests.

#ifdef _WIN32
class DxilModuleTest {
#else
class DxilModuleTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(DxilModuleTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  dxc::DxcDllSupport m_dllSupport;

  // Basic loading tests.
  TEST_METHOD(LoadDxilModule_1_0)
  TEST_METHOD(LoadDxilModule_1_1)
  TEST_METHOD(LoadDxilModule_1_2)

  // Precise query tests.
  TEST_METHOD(Precise1)
  TEST_METHOD(Precise2)
  TEST_METHOD(Precise3)
  TEST_METHOD(Precise4)
  TEST_METHOD(Precise5)
  TEST_METHOD(Precise6)
  TEST_METHOD(Precise7)
};

bool DxilModuleTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Compilation and dxil module loading support.

namespace {
class Compiler {
public:
  Compiler(dxc::DxcDllSupport &dll) 
    : m_dllSupport(dll) 
    , m_msf(CreateMSFileSystem())
    , m_pts(m_msf.get())
  {
    m_ver.Initialize(m_dllSupport);
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  }

  bool SkipDxil_Test(unsigned major, unsigned minor) {
    return m_ver.SkipDxilVersion(major, minor);
  }
  
  IDxcOperationResult *Compile(const char *program, LPCWSTR shaderModel = L"ps_6_0") {
    return Compile(program, shaderModel, {}, {});
  }

  IDxcOperationResult *Compile(const char *program, LPCWSTR shaderModel, const std::vector<LPCWSTR> &arguments, const std::vector<DxcDefine> defs ) {
    Utf8ToBlob(m_dllSupport, program, &pCodeBlob);
    VERIFY_SUCCEEDED(pCompiler->Compile(pCodeBlob, L"hlsl.hlsl", L"main",
      shaderModel,
      const_cast<LPCWSTR *>(arguments.data()), arguments.size(),
      defs.data(), defs.size(),
      nullptr, &pCompileResult));

    return pCompileResult;
  }

  std::string Disassemble() {
    CComPtr<IDxcBlob> pBlob;
    CheckOperationSucceeded(pCompileResult, &pBlob);
    return DisassembleProgram(m_dllSupport, pBlob);
  }

  DxilModule &GetDxilModule() {
    // Make sure we compiled successfully.
    CComPtr<IDxcBlob> pBlob;
    CheckOperationSucceeded(pCompileResult, &pBlob);
    
    // Verify we have a valid dxil container.
    const DxilContainerHeader *pContainer =
      IsDxilContainerLike(pBlob->GetBufferPointer(), pBlob->GetBufferSize());
    VERIFY_IS_NOT_NULL(pContainer);
    VERIFY_IS_TRUE(IsValidDxilContainer(pContainer, pBlob->GetBufferSize()));
        
    // Get Dxil part from container.
    DxilPartIterator it = std::find_if(begin(pContainer), end(pContainer), DxilPartIsType(DFCC_DXIL));
    VERIFY_IS_FALSE(it == end(pContainer));
    
    const DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
    VERIFY_IS_TRUE(IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize));
        
    // Get a pointer to the llvm bitcode.
    const char *pIL;
    uint32_t pILLength;
    GetDxilProgramBitcode(pProgramHeader, &pIL, &pILLength);
      
    // Parse llvm bitcode into a module.
    std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf(
          llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(pIL, pILLength), "", false));
    llvm::ErrorOr<std::unique_ptr<llvm::Module>>
      pModule(llvm::parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), m_llvmContext));
    if (std::error_code ec = pModule.getError()) {
      VERIFY_FAIL();
    }
    m_module = std::move(pModule.get());

    // Grab the dxil module;
    DxilModule *DM = DxilModule::TryGetDxilModule(m_module.get());
    VERIFY_IS_NOT_NULL(DM);
    return *DM;
  }

private:
  static ::llvm::sys::fs::MSFileSystem *CreateMSFileSystem() {
    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    return msfPtr;
  }

  dxc::DxcDllSupport &m_dllSupport;
  VersionSupportInfo m_ver;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pCodeBlob;
  CComPtr<IDxcOperationResult> pCompileResult;
  llvm::LLVMContext m_llvmContext;
  std::unique_ptr<llvm::Module> m_module;
  std::unique_ptr<::llvm::sys::fs::MSFileSystem> m_msf;
  ::llvm::sys::fs::AutoPerThreadSystem m_pts;
};
}

///////////////////////////////////////////////////////////////////////////////
// Unit Test Implementation
TEST_F(DxilModuleTest, LoadDxilModule_1_0) {
  Compiler c(m_dllSupport);
  c.Compile(
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n"
    ,
    L"ps_6_0"
  );

  // Basic sanity check on dxil version in dxil module.
  DxilModule &DM = c.GetDxilModule();
  unsigned vMajor, vMinor;
  DM.GetDxilVersion(vMajor, vMinor);
  VERIFY_IS_TRUE(vMajor == 1);
  VERIFY_IS_TRUE(vMinor == 0);
}

TEST_F(DxilModuleTest, LoadDxilModule_1_1) {
  Compiler c(m_dllSupport);
  if (c.SkipDxil_Test(1,1)) return;
  c.Compile(
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n"
    ,
    L"ps_6_1"
  );

  // Basic sanity check on dxil version in dxil module.
  DxilModule &DM = c.GetDxilModule();
  unsigned vMajor, vMinor;
  DM.GetDxilVersion(vMajor, vMinor);
  VERIFY_IS_TRUE(vMajor == 1);
  VERIFY_IS_TRUE(vMinor == 1);
}

TEST_F(DxilModuleTest, LoadDxilModule_1_2) {
  Compiler c(m_dllSupport);
  if (c.SkipDxil_Test(1,2)) return;
  c.Compile(
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n"
    ,
    L"ps_6_2"
  );

  // Basic sanity check on dxil version in dxil module.
  DxilModule &DM = c.GetDxilModule();
  unsigned vMajor, vMinor;
  DM.GetDxilVersion(vMajor, vMinor);
  VERIFY_IS_TRUE(vMajor == 1);
  VERIFY_IS_TRUE(vMinor == 2);
}

TEST_F(DxilModuleTest, Precise1) {
  Compiler c(m_dllSupport);
  c.Compile(
    "precise float main(float x : X, float y : Y) : SV_Target {\n"
    "  return sqrt(x) + y;\n"
    "}\n"
  );

  // Make sure sqrt and add are marked precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FAdd(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 2);
}

TEST_F(DxilModuleTest, Precise2) {
  Compiler c(m_dllSupport);
  c.Compile(
    "float main(float x : X, float y : Y) : SV_Target {\n"
    "  return sqrt(x) + y;\n"
    "}\n"
  );

  // Make sure sqrt and add are not marked precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FAdd(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 2);
}

TEST_F(DxilModuleTest, Precise3) {
  // TODO: Enable this test when precise metadata is inserted for Gis.
  if (const bool GisIsBroken = true) return;
  Compiler c(m_dllSupport);
  c.Compile(
    "float main(float x : X, float y : Y) : SV_Target {\n"
    "  return sqrt(x) + y;\n"
    "}\n",
    L"ps_6_0",
    { L"/Gis" }, {}
  );

  // Make sure sqrt and add are marked precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FAdd(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 2);
}

TEST_F(DxilModuleTest, Precise4) {
  Compiler c(m_dllSupport);
  c.Compile(
    "float main(float x : X, float y : Y) : SV_Target {\n"
    "  precise float sx = 1 / sqrt(x);\n"
    "  return sx + y;\n"
    "}\n"
  );

  // Make sure sqrt and div are marked precise, and add is not.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FDiv(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FAdd(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 3);
}

TEST_F(DxilModuleTest, Precise5) {
  Compiler c(m_dllSupport);
  c.Compile(
    "float C[10];\n"
    "float main(float x : X, float y : Y, int i : I) : SV_Target {\n"
    "  float A[2];\n"
    "  A[0] = x;\n"
    "  A[1] = y;\n"
    "  return A[i] + C[i];\n"
    "}\n"
  );

  // Make sure load and extract value are not reported as precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (LlvmInst_ExtractValue(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_Load(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FAdd(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 3);
}

TEST_F(DxilModuleTest, Precise6) {
  Compiler c(m_dllSupport);
  c.Compile(
    "precise float2 main(float2 x : A, float2 y : B) : SV_Target {\n"
    "  return sqrt(x * y);\n"
    "}\n"
  );

  // Make sure sqrt and mul are marked precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FMul(Inst)) {
      numChecks++;
      VERIFY_IS_TRUE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 4);
}

TEST_F(DxilModuleTest, Precise7) {
  Compiler c(m_dllSupport);
  c.Compile(
    "float2 main(float2 x : A, float2 y : B) : SV_Target {\n"
    "  return sqrt(x * y);\n"
    "}\n"
  );

  // Make sure sqrt and mul are not marked precise.
  DxilModule &DM = c.GetDxilModule();
  Function *F = DM.GetEntryFunction();
  int numChecks = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *Inst = &*I;
    if (DxilInst_Sqrt(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
    else if (LlvmInst_FMul(Inst)) {
      numChecks++;
      VERIFY_IS_FALSE(DM.IsPrecise(Inst));
    }
  }
  VERIFY_ARE_EQUAL(numChecks, 4);
}
