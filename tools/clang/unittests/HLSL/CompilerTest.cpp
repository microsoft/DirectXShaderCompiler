///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CompilerTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the compiler API.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <cfloat>
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#ifdef _WIN32
#include <atlfile.h>
#include "dia2.h"
#endif

#include "HLSLTestData.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include <fstream>
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"

using namespace std;
using namespace hlsl_test;

// Aligned to SymTagEnum.
const char *SymTagEnumText[] =
{
  "Null", // SymTagNull
  "Exe", // SymTagExe
  "Compiland", // SymTagCompiland
  "CompilandDetails", // SymTagCompilandDetails
  "CompilandEnv", // SymTagCompilandEnv
  "Function", // SymTagFunction
  "Block", // SymTagBlock
  "Data", // SymTagData
  "Annotation", // SymTagAnnotation
  "Label", // SymTagLabel
  "PublicSymbol", // SymTagPublicSymbol
  "UDT", // SymTagUDT
  "Enum", // SymTagEnum
  "FunctionType", // SymTagFunctionType
  "PointerType", // SymTagPointerType
  "ArrayType", // SymTagArrayType
  "BaseType", // SymTagBaseType
  "Typedef", // SymTagTypedef
  "BaseClass", // SymTagBaseClass
  "Friend", // SymTagFriend
  "FunctionArgType", // SymTagFunctionArgType
  "FuncDebugStart", // SymTagFuncDebugStart
  "FuncDebugEnd", // SymTagFuncDebugEnd
  "UsingNamespace", // SymTagUsingNamespace
  "VTableShape", // SymTagVTableShape
  "VTable", // SymTagVTable
  "Custom", // SymTagCustom
  "Thunk", // SymTagThunk
  "CustomType", // SymTagCustomType
  "ManagedType", // SymTagManagedType
  "Dimension", // SymTagDimension
  "CallSite", // SymTagCallSite
  "InlineSite", // SymTagInlineSite
  "BaseInterface", // SymTagBaseInterface
  "VectorType", // SymTagVectorType
  "MatrixType", // SymTagMatrixType
  "HLSLType", // SymTagHLSLType
  "Caller", // SymTagCaller
  "Callee", // SymTagCallee
  "Export", // SymTagExport
  "HeapAllocationSite", // SymTagHeapAllocationSite
  "CoffGroup", // SymTagCoffGroup
};

// Aligned to LocationType.
const char *LocationTypeText[] =
{
  "Null",
  "Static",
  "TLS",
  "RegRel",
  "ThisRel",
  "Enregistered",
  "BitField",
  "Slot",
  "IlRel",
  "MetaData",
  "Constant",
};

// Aligned to DataKind.
const char *DataKindText[] =
{
  "Unknown",
  "Local",
  "StaticLocal",
  "Param",
  "ObjectPtr",
  "FileStatic",
  "Global",
  "Member",
  "StaticMember",
  "Constant",
};

// Aligned to UdtKind.
const char *UdtKindText[] =
{
  "Struct",
  "Class",
  "Union",
  "Interface",
};

class TestIncludeHandler : public IDxcIncludeHandler {
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  dxc::DxcDllSupport &m_dllSupport;
  HRESULT m_defaultErrorCode = E_FAIL;
  TestIncludeHandler(dxc::DxcDllSupport &dllSupport) : m_dwRef(0), m_dllSupport(dllSupport), callIndex(0) { }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this,  iid, ppvObject);
  }

  struct LoadSourceCallInfo {
    std::wstring Filename;     // Filename as written in #include statement
    LoadSourceCallInfo(LPCWSTR pFilename) :
      Filename(pFilename) { }
  };
  std::vector<LoadSourceCallInfo> CallInfos;
  std::wstring GetAllFileNames() const {
    std::wstringstream s;
    for (size_t i = 0; i < CallInfos.size(); ++i) {
      s << CallInfos[i].Filename << ';';
    }
    return s.str();
  }
  struct LoadSourceCallResult {
    HRESULT hr;
    std::string source;
    UINT32 codePage;
    LoadSourceCallResult() : hr(E_FAIL), codePage(0) { }
    LoadSourceCallResult(const char *pSource, UINT32 codePage = CP_UTF8) : hr(S_OK), source(pSource), codePage(codePage) { }
  };
  std::vector<LoadSourceCallResult> CallResults;
  size_t callIndex;

  HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                   // Filename as written in #include statement
    _COM_Outptr_ IDxcBlob **ppIncludeSource   // Resultant source object for included file
    ) override {
    CallInfos.push_back(LoadSourceCallInfo(pFilename));

    *ppIncludeSource = nullptr;
    if (callIndex >= CallResults.size()) {
      return m_defaultErrorCode;
    }
    if (FAILED(CallResults[callIndex].hr)) {
      return CallResults[callIndex++].hr;
    }
    MultiByteStringToBlob(m_dllSupport, CallResults[callIndex].source,
                          CallResults[callIndex].codePage, ppIncludeSource);
    return CallResults[callIndex++].hr;
  }
};

#ifdef _WIN32
class CompilerTest {
#else
class CompilerTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(CompilerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDebugThenDIPresent)
  TEST_METHOD(CompileDebugLines)

  TEST_METHOD(CompileWhenDefinesThenApplied)
  TEST_METHOD(CompileWhenDefinesManyThenApplied)
  TEST_METHOD(CompileWhenEmptyThenFails)
  TEST_METHOD(CompileWhenIncorrectThenFails)
  TEST_METHOD(CompileWhenWorksThenDisassembleWorks)
  TEST_METHOD(CompileWhenDebugWorksThenStripDebug)
  TEST_METHOD(CompileWhenWorksThenAddRemovePrivate)
  TEST_METHOD(CompileThenAddCustomDebugName)
  TEST_METHOD(CompileWithRootSignatureThenStripRootSignature)

  TEST_METHOD(CompileWhenIncludeThenLoadInvoked)
  TEST_METHOD(CompileWhenIncludeThenLoadUsed)
  TEST_METHOD(CompileWhenIncludeAbsoluteThenLoadAbsolute)
  TEST_METHOD(CompileWhenIncludeLocalThenLoadRelative)
  TEST_METHOD(CompileWhenIncludeSystemThenLoadNotRelative)
  TEST_METHOD(CompileWhenIncludeSystemMissingThenLoadAttempt)
  TEST_METHOD(CompileWhenIncludeFlagsThenIncludeUsed)
  TEST_METHOD(CompileWhenIncludeMissingThenFail)
  TEST_METHOD(CompileWhenIncludeHasPathThenOK)
  TEST_METHOD(CompileWhenIncludeEmptyThenOK)

  TEST_METHOD(CompileWhenODumpThenPassConfig)
  TEST_METHOD(CompileWhenODumpThenOptimizerMatch)
  TEST_METHOD(CompileWhenVdThenProducesDxilContainer)

  TEST_METHOD(CompileWhenNoMemThenOOM)
  TEST_METHOD(CompileWhenShaderModelMismatchAttributeThenFail)
  TEST_METHOD(CompileBadHlslThenFail)
  TEST_METHOD(CompileLegacyShaderModelThenFail)
  TEST_METHOD(CompileWhenRecursiveAlbeitStaticTermThenFail)

  TEST_METHOD(CompileWhenRecursiveThenFail)

  TEST_METHOD(CompileHlsl2015ThenFail)
  TEST_METHOD(CompileHlsl2016ThenOK)
  TEST_METHOD(CompileHlsl2017ThenOK)
  TEST_METHOD(CompileHlsl2018ThenOK)
  TEST_METHOD(CompileHlsl2019ThenFail)
  TEST_METHOD(CompileCBufferTBufferASTDump)

  TEST_METHOD(DiaLoadBadBitcodeThenFail)
  TEST_METHOD(DiaLoadDebugThenOK)
  TEST_METHOD(DiaTableIndexThenOK)

  TEST_METHOD(PixMSAAToSample0)
  TEST_METHOD(PixRemoveDiscards)
  TEST_METHOD(PixPixelCounter)
  TEST_METHOD(PixPixelCounterEarlyZ)
  TEST_METHOD(PixPixelCounterNoSvPosition)
  TEST_METHOD(PixPixelCounterAddPixelCost)
  TEST_METHOD(PixConstantColor)
  TEST_METHOD(PixConstantColorInt)
  TEST_METHOD(PixConstantColorMRT)
  TEST_METHOD(PixConstantColorUAVs)
  TEST_METHOD(PixConstantColorOtherSIVs)
  TEST_METHOD(PixConstantColorFromCB)
  TEST_METHOD(PixConstantColorFromCBint)
  TEST_METHOD(PixForceEarlyZ)
  TEST_METHOD(PixDebugBasic)
  TEST_METHOD(PixDebugUAVSize)
  TEST_METHOD(PixDebugGSParameters)
  TEST_METHOD(PixDebugPSParameters)
  TEST_METHOD(PixDebugVSParameters)
  TEST_METHOD(PixDebugCSParameters)
  TEST_METHOD(PixDebugFlowControl)
  TEST_METHOD(PixDebugPreexistingSVPosition)
  TEST_METHOD(PixDebugPreexistingSVVertex)
  TEST_METHOD(PixDebugPreexistingSVInstance)
  TEST_METHOD(PixAccessTracking)

  TEST_METHOD(CodeGenAbs1)
  TEST_METHOD(CodeGenAbs2)
  TEST_METHOD(CodeGenAllLit)
  TEST_METHOD(CodeGenAllocaAtEntryBlk)
  TEST_METHOD(CodeGenAddUint64)
  TEST_METHOD(CodeGenArrayArg)
  TEST_METHOD(CodeGenArrayArg2)
  TEST_METHOD(CodeGenArrayArg3)
  TEST_METHOD(CodeGenArrayOfStruct)
  TEST_METHOD(CodeGenAsUint)
  TEST_METHOD(CodeGenAsUint2)
  TEST_METHOD(CodeGenAtomic)
  TEST_METHOD(CodeGenAtomic2)
  TEST_METHOD(CodeGenAttributeAtVertex)
  TEST_METHOD(CodeGenAttributeAtVertexNoOpt)
  TEST_METHOD(CodeGenBarycentrics)
  TEST_METHOD(CodeGenBarycentrics1)
  TEST_METHOD(CodeGenBarycentricsThreeSV)
  TEST_METHOD(CodeGenBinary1)
  TEST_METHOD(CodeGenBitCast)
  TEST_METHOD(CodeGenBitCast16Bits)
  TEST_METHOD(CodeGenBoolComb)
  TEST_METHOD(CodeGenBoolSvTarget)
  TEST_METHOD(CodeGenCalcLod2DArray)
  TEST_METHOD(CodeGenCall1)
  TEST_METHOD(CodeGenCall3)
  TEST_METHOD(CodeGenCast1)
  TEST_METHOD(CodeGenCast2)
  TEST_METHOD(CodeGenCast3)
  TEST_METHOD(CodeGenCast4)
  TEST_METHOD(CodeGenCast5)
  TEST_METHOD(CodeGenCast6)
  TEST_METHOD(CodeGenCast7)
  TEST_METHOD(CodeGenCbuf_init_static)
  TEST_METHOD(CodeGenCbufferCopy)
  TEST_METHOD(CodeGenCbufferCopy1)
  TEST_METHOD(CodeGenCbufferCopy2)
  TEST_METHOD(CodeGenCbufferCopy3)
  TEST_METHOD(CodeGenCbufferCopy4)
  TEST_METHOD(CodeGenCbufferWithFunction)
  TEST_METHOD(CodeGenCbufferWithFunctionCopy)
  TEST_METHOD(CodeGenCbuffer_unused)
  TEST_METHOD(CodeGenCbuffer1_50)
  TEST_METHOD(CodeGenCbuffer1_51)
  TEST_METHOD(CodeGenCbuffer2_50)
  TEST_METHOD(CodeGenCbuffer2_51)
  TEST_METHOD(CodeGenCbuffer3_50)
  TEST_METHOD(CodeGenCbuffer3_51)
  TEST_METHOD(CodeGenCbuffer5_51)
  TEST_METHOD(CodeGenCbuffer6_51)
  TEST_METHOD(CodeGenCbuffer64Types)
  TEST_METHOD(CodeGenCbufferAlloc)
  TEST_METHOD(CodeGenCbufferAllocLegacy)
  TEST_METHOD(CodeGenCbufferHalf)
  TEST_METHOD(CodeGenCbufferHalfStruct)
  TEST_METHOD(CodeGenCbufferInLoop)
  TEST_METHOD(CodeGenCbufferInt16)
  TEST_METHOD(CodeGenCbufferInt16Struct)
  TEST_METHOD(CodeGenCbufferMinPrec)
  TEST_METHOD(CodeGenClass)
  TEST_METHOD(CodeGenClip)
  TEST_METHOD(CodeGenClipPlanes)
  TEST_METHOD(CodeGenConstoperand1)
  TEST_METHOD(CodeGenConstMat)
  TEST_METHOD(CodeGenConstMat2)
  TEST_METHOD(CodeGenConstMat3)
  TEST_METHOD(CodeGenConstMat4)
  TEST_METHOD(CodeGenCorrectDelay)
  TEST_METHOD(CodeGenDataLayout)
  TEST_METHOD(CodeGenDataLayoutHalf)
  TEST_METHOD(CodeGenDiscard)
  TEST_METHOD(CodeGenDivZero)
  TEST_METHOD(CodeGenDot1)
  TEST_METHOD(CodeGenDynamic_Resources)
  TEST_METHOD(CodeGenEffectSkip)
  TEST_METHOD(CodeGenEliminateDynamicIndexing)
  TEST_METHOD(CodeGenEliminateDynamicIndexing2)
  TEST_METHOD(CodeGenEliminateDynamicIndexing3)
  TEST_METHOD(CodeGenEliminateDynamicIndexing4)
  TEST_METHOD(CodeGenEliminateDynamicIndexing6)
  TEST_METHOD(CodeGenEmpty)
  TEST_METHOD(CodeGenEmptyStruct)
  TEST_METHOD(CodeGenEnum1)
  TEST_METHOD(CodeGenEnum2)
  TEST_METHOD(CodeGenEnum3)
  TEST_METHOD(CodeGenEnum4)
  TEST_METHOD(CodeGenEnum5)
  TEST_METHOD(CodeGenEnum6)
  TEST_METHOD(CodeGenEarlyDepthStencil)
  TEST_METHOD(CodeGenEval)
  TEST_METHOD(CodeGenEvalInvalid)
  TEST_METHOD(CodeGenEvalMat)
  TEST_METHOD(CodeGenEvalMatMember)
  TEST_METHOD(CodeGenEvalPos)
  TEST_METHOD(CodeGenExternRes)
  TEST_METHOD(CodeGenExpandTrig)
  TEST_METHOD(CodeGenFloatCast)
  TEST_METHOD(CodeGenFloatingPointEnvironment)
  TEST_METHOD(CodeGenFloatToBool)
  TEST_METHOD(CodeGenFirstbitHi)
  TEST_METHOD(CodeGenFirstbitLo)
  TEST_METHOD(CodeGenFixedWidthTypes)
  TEST_METHOD(CodeGenFixedWidthTypes16Bit)
  TEST_METHOD(CodeGenFloatMaxtessfactor)
  TEST_METHOD(CodeGenFModPS)
  TEST_METHOD(CodeGenFuncCast)
  TEST_METHOD(CodeGenFunctionalCast)
  TEST_METHOD(CodeGenFunctionAttribute)
  TEST_METHOD(CodeGenGather)
  TEST_METHOD(CodeGenGatherCmp)
  TEST_METHOD(CodeGenGatherCubeOffset)
  TEST_METHOD(CodeGenGatherOffset)
  TEST_METHOD(CodeGenGepZeroIdx)
  TEST_METHOD(CodeGenGloballyCoherent)
  TEST_METHOD(CodeGenI32ColIdx)
  TEST_METHOD(CodeGenIcb1)
  TEST_METHOD(CodeGenIf1)
  TEST_METHOD(CodeGenIf2)
  TEST_METHOD(CodeGenIf3)
  TEST_METHOD(CodeGenIf4)
  TEST_METHOD(CodeGenIf5)
  TEST_METHOD(CodeGenIf6)
  TEST_METHOD(CodeGenIf7)
  TEST_METHOD(CodeGenIf8)
  TEST_METHOD(CodeGenIf9)
  TEST_METHOD(CodeGenImm0)
  TEST_METHOD(CodeGenInclude)
  TEST_METHOD(CodeGenIncompletePos)
  TEST_METHOD(CodeGenIndexableinput1)
  TEST_METHOD(CodeGenIndexableinput2)
  TEST_METHOD(CodeGenIndexableinput3)
  TEST_METHOD(CodeGenIndexableinput4)
  TEST_METHOD(CodeGenIndexableoutput1)
  TEST_METHOD(CodeGenIndexabletemp1)
  TEST_METHOD(CodeGenIndexabletemp2)
  TEST_METHOD(CodeGenIndexabletemp3)
  TEST_METHOD(CodeGenInitListType)
  TEST_METHOD(CodeGenInoutSE)
  TEST_METHOD(CodeGenInout1)
  TEST_METHOD(CodeGenInout2)
  TEST_METHOD(CodeGenInout3)
  TEST_METHOD(CodeGenInout4)
  TEST_METHOD(CodeGenInout5)
  TEST_METHOD(CodeGenInput1)
  TEST_METHOD(CodeGenInput2)
  TEST_METHOD(CodeGenInput3)
  TEST_METHOD(CodeGenInt16Op)
  TEST_METHOD(CodeGenInt16OpBits)
  TEST_METHOD(CodeGenIntrinsic1)
  TEST_METHOD(CodeGenIntrinsic1Minprec)
  TEST_METHOD(CodeGenIntrinsic2)
  TEST_METHOD(CodeGenIntrinsic2Minprec)
  TEST_METHOD(CodeGenIntrinsic3_even)
  TEST_METHOD(CodeGenIntrinsic3_integer)
  TEST_METHOD(CodeGenIntrinsic3_odd)
  TEST_METHOD(CodeGenIntrinsic3_pow2)
  TEST_METHOD(CodeGenIntrinsic4)
  TEST_METHOD(CodeGenIntrinsic4_dbg)
  TEST_METHOD(CodeGenIntrinsic5)
  TEST_METHOD(CodeGenIntrinsic5Minprec)
  TEST_METHOD(CodeGenInvalidInputOutputTypes)
  TEST_METHOD(CodeGenLegacyStruct)
  TEST_METHOD(CodeGenLibCsEntry)
  TEST_METHOD(CodeGenLibCsEntry2)
  TEST_METHOD(CodeGenLibCsEntry3)
  TEST_METHOD(CodeGenLibEntries)
  TEST_METHOD(CodeGenLibEntries2)
  TEST_METHOD(CodeGenLibNoAlias)
  TEST_METHOD(CodeGenLibResource)
  TEST_METHOD(CodeGenLibUnusedFunc)
  TEST_METHOD(CodeGenLitInParen)
  TEST_METHOD(CodeGenLiteralShift)
  TEST_METHOD(CodeGenLiveness1)
  TEST_METHOD(CodeGenLocalRes1)
  TEST_METHOD(CodeGenLocalRes4)
  TEST_METHOD(CodeGenLocalRes7)
  TEST_METHOD(CodeGenLocalRes7Dbg)
  TEST_METHOD(CodeGenLoop1)
  TEST_METHOD(CodeGenLoop2)
  TEST_METHOD(CodeGenLoop3)
  TEST_METHOD(CodeGenLoop4)
  TEST_METHOD(CodeGenLoop5)
  TEST_METHOD(CodeGenLoop6)
  TEST_METHOD(CodeGenMatParam)
  TEST_METHOD(CodeGenMatParam2)
 // TEST_METHOD(CodeGenMatParam3)
  TEST_METHOD(CodeGenMatArrayOutput)
  TEST_METHOD(CodeGenMatArrayOutput2)
  TEST_METHOD(CodeGenMatElt)
  TEST_METHOD(CodeGenMatInit)
  TEST_METHOD(CodeGenMatMulMat)
  TEST_METHOD(CodeGenMatOps)
  TEST_METHOD(CodeGenMatInStruct)
  TEST_METHOD(CodeGenMatInStructRet)
  TEST_METHOD(CodeGenMatIn)
  TEST_METHOD(CodeGenMatIn1)
  TEST_METHOD(CodeGenMatIn2)
  TEST_METHOD(CodeGenMatOut)
  TEST_METHOD(CodeGenMatOut1)
  TEST_METHOD(CodeGenMatOut2)
  TEST_METHOD(CodeGenMatSubscript)
  TEST_METHOD(CodeGenMatSubscript2)
  TEST_METHOD(CodeGenMatSubscript3)
  TEST_METHOD(CodeGenMatSubscript4)
  TEST_METHOD(CodeGenMatSubscript5)
  TEST_METHOD(CodeGenMatSubscript6)
  TEST_METHOD(CodeGenMatSubscript7)
  TEST_METHOD(CodeGenMaxMin)
  TEST_METHOD(CodeGenMinprec1)
  TEST_METHOD(CodeGenMinprec2)
  TEST_METHOD(CodeGenMinprec3)
  TEST_METHOD(CodeGenMinprec4)
  TEST_METHOD(CodeGenMinprec5)
  TEST_METHOD(CodeGenMinprec6)
  TEST_METHOD(CodeGenMinprec7)
  TEST_METHOD(CodeGenMinprecCoord)
  TEST_METHOD(CodeGenModf)
  TEST_METHOD(CodeGenMinprecCast)
  TEST_METHOD(CodeGenMinprecImm)
  TEST_METHOD(CodeGenMultiUAVLoad1)
  TEST_METHOD(CodeGenMultiUAVLoad2)
  TEST_METHOD(CodeGenMultiUAVLoad3)
  TEST_METHOD(CodeGenMultiUAVLoad4)
  TEST_METHOD(CodeGenMultiUAVLoad5)
  TEST_METHOD(CodeGenMultiUAVLoad6)
  TEST_METHOD(CodeGenMultiUAVLoad7)
  TEST_METHOD(CodeGenMultiStream)
  TEST_METHOD(CodeGenMultiStream2)
  TEST_METHOD(CodeGenNeg1)
  TEST_METHOD(CodeGenNeg2)
  TEST_METHOD(CodeGenNegabs1)
  TEST_METHOD(CodeGenNoise)
  TEST_METHOD(CodeGenNonUniform)
  TEST_METHOD(CodeGenOptForNoOpt)
  TEST_METHOD(CodeGenOptForNoOpt2)
  TEST_METHOD(CodeGenOptionGis)
  TEST_METHOD(CodeGenOptionWX)
  TEST_METHOD(CodeGenOutput1)
  TEST_METHOD(CodeGenOutput2)
  TEST_METHOD(CodeGenOutput3)
  TEST_METHOD(CodeGenOutput4)
  TEST_METHOD(CodeGenOutput5)
  TEST_METHOD(CodeGenOutput6)
  TEST_METHOD(CodeGenOutputArray)
  TEST_METHOD(CodeGenPassthrough1)
  TEST_METHOD(CodeGenPassthrough2)
  TEST_METHOD(CodeGenPrecise1)
  TEST_METHOD(CodeGenPrecise2)
  TEST_METHOD(CodeGenPrecise3)
  TEST_METHOD(CodeGenPrecise4)
  TEST_METHOD(CodeGenPreciseOnCall)
  TEST_METHOD(CodeGenPreciseOnCallNot)
  TEST_METHOD(CodeGenPreserveAllOutputs)
  TEST_METHOD(CodeGenRaceCond2)
  TEST_METHOD(CodeGenRaw_Buf1)
  TEST_METHOD(CodeGenRaw_Buf2)
  TEST_METHOD(CodeGenRaw_Buf3)
  TEST_METHOD(CodeGenRaw_Buf4)
  TEST_METHOD(CodeGenRaw_Buf5)
  TEST_METHOD(CodeGenRcp1)
  TEST_METHOD(CodeGenReadFromOutput)
  TEST_METHOD(CodeGenReadFromOutput2)
  TEST_METHOD(CodeGenReadFromOutput3)
  TEST_METHOD(CodeGenRedundantinput1)
  TEST_METHOD(CodeGenRes64bit)
  TEST_METHOD(CodeGenRovs)
  TEST_METHOD(CodeGenRValAssign)
  TEST_METHOD(CodeGenRValSubscript)
  TEST_METHOD(CodeGenSample1)
  TEST_METHOD(CodeGenSample2)
  TEST_METHOD(CodeGenSample3)
  TEST_METHOD(CodeGenSample4)
  TEST_METHOD(CodeGenSample5)
  TEST_METHOD(CodeGenSampleBias)
  TEST_METHOD(CodeGenSampleCmp)
  TEST_METHOD(CodeGenSampleCmpLZ)
  TEST_METHOD(CodeGenSampleCmpLZ2)
  TEST_METHOD(CodeGenSampleGrad)
  TEST_METHOD(CodeGenSampleL)
  TEST_METHOD(CodeGenSaturate1)
  TEST_METHOD(CodeGenScalarOnVecIntrinsic)
  TEST_METHOD(CodeGenScalarToVec)
  TEST_METHOD(CodeGenSelectObj)
  TEST_METHOD(CodeGenSelectObj2)
  TEST_METHOD(CodeGenSelectObj3)
  TEST_METHOD(CodeGenSelectObj4)
  TEST_METHOD(CodeGenSelectObj5)
  TEST_METHOD(CodeGenSelfCopy)
  TEST_METHOD(CodeGenSelMat)
  TEST_METHOD(CodeGenSignaturePacking)
  TEST_METHOD(CodeGenSignaturePackingByWidth)
  TEST_METHOD(CodeGenShaderAttr)
  TEST_METHOD(CodeGenShare_Mem_Dbg)
  TEST_METHOD(CodeGenShare_Mem_Phi)
  TEST_METHOD(CodeGenShare_Mem1)
  TEST_METHOD(CodeGenShare_Mem2)
  TEST_METHOD(CodeGenShare_Mem2Dim)
  TEST_METHOD(CodeGenShift)
  TEST_METHOD(CodeGenShortCircuiting0)
  TEST_METHOD(CodeGenShortCircuiting1)
  TEST_METHOD(CodeGenShortCircuiting2)
  TEST_METHOD(CodeGenShortCircuiting3)
  TEST_METHOD(CodeGenSimpleDS1)
  TEST_METHOD(CodeGenSimpleGS1)
  TEST_METHOD(CodeGenSimpleGS2)
  TEST_METHOD(CodeGenSimpleGS3)
  TEST_METHOD(CodeGenSimpleGS4)
  TEST_METHOD(CodeGenSimpleGS5)
  TEST_METHOD(CodeGenSimpleGS6)
  TEST_METHOD(CodeGenSimpleGS7)
  TEST_METHOD(CodeGenSimpleGS11)
  TEST_METHOD(CodeGenSimpleGS12)
  TEST_METHOD(CodeGenSimpleHS1)
  TEST_METHOD(CodeGenSimpleHS2)
  TEST_METHOD(CodeGenSimpleHS3)
  TEST_METHOD(CodeGenSimpleHS4)
  TEST_METHOD(CodeGenSimpleHS5)
  TEST_METHOD(CodeGenSimpleHS6)
  TEST_METHOD(CodeGenSimpleHS7)
  TEST_METHOD(CodeGenSimpleHS8)
  TEST_METHOD(CodeGenSimpleHS9)
  TEST_METHOD(CodeGenSimpleHS10)
  TEST_METHOD(CodeGenSimpleHS11)
  TEST_METHOD(CodeGenSMFail)
  TEST_METHOD(CodeGenSrv_Ms_Load1)
  TEST_METHOD(CodeGenSrv_Ms_Load2)
  TEST_METHOD(CodeGenSrv_Typed_Load1)
  TEST_METHOD(CodeGenSrv_Typed_Load2)
  TEST_METHOD(CodeGenStaticConstGlobal)
  TEST_METHOD(CodeGenStaticConstGlobal2)
  TEST_METHOD(CodeGenStaticGlobals)
  TEST_METHOD(CodeGenStaticGlobals2)
  TEST_METHOD(CodeGenStaticGlobals3)
  TEST_METHOD(CodeGenStaticGlobals4)
  TEST_METHOD(CodeGenStaticGlobals5)
  TEST_METHOD(CodeGenStaticMatrix)
  TEST_METHOD(CodeGenStaticResource)
  TEST_METHOD(CodeGenStaticResource2)
  TEST_METHOD(CodeGenStruct_Buf1)
  TEST_METHOD(CodeGenStruct_Buf2)
  TEST_METHOD(CodeGenStruct_Buf3)
  TEST_METHOD(CodeGenStruct_Buf4)
  TEST_METHOD(CodeGenStruct_Buf5)
  TEST_METHOD(CodeGenStruct_Buf6)
  TEST_METHOD(CodeGenStruct_Buf_New_Layout)
  TEST_METHOD(CodeGenStruct_BufHasCounter)
  TEST_METHOD(CodeGenStruct_BufHasCounter2)
  TEST_METHOD(CodeGenStructArray)
  TEST_METHOD(CodeGenStructCast)
  TEST_METHOD(CodeGenStructCast2)
  TEST_METHOD(CodeGenStructInBuffer)
  TEST_METHOD(CodeGenStructInBuffer2)
  TEST_METHOD(CodeGenStructInBuffer3)
  TEST_METHOD(CodeGenSwitchFloat)
  TEST_METHOD(CodeGenSwitch1)
  TEST_METHOD(CodeGenSwitch2)
  TEST_METHOD(CodeGenSwitch3)
  TEST_METHOD(CodeGenSwizzle1)
  TEST_METHOD(CodeGenSwizzle2)
  TEST_METHOD(CodeGenSwizzleAtomic)
  TEST_METHOD(CodeGenSwizzleAtomic2)
  TEST_METHOD(CodeGenSwizzleIndexing)
  TEST_METHOD(CodeGenTempDbgInfo)
  TEST_METHOD(CodeGenTemp1)
  TEST_METHOD(CodeGenTemp2)
  TEST_METHOD(CodeGenTexSubscript)
  TEST_METHOD(CodeGenUav_Raw1)
  TEST_METHOD(CodeGenUav_Typed_Load_Store1)
  TEST_METHOD(CodeGenUav_Typed_Load_Store2)
  TEST_METHOD(CodeGenUav_Typed_Load_Store3)
  TEST_METHOD(CodeGenUint64_1)
  TEST_METHOD(CodeGenUint64_2)
  TEST_METHOD(CodeGenUintSample)
  TEST_METHOD(CodeGenUmaxObjectAtomic)
  TEST_METHOD(CodeGenUnrollDbg)
  TEST_METHOD(CodeGenUnsignedShortHandMatrixVector)
  TEST_METHOD(CodeGenUnusedFunc)
  TEST_METHOD(CodeGenUnusedCB)
  TEST_METHOD(CodeGenUpdateCounter)
  TEST_METHOD(CodeGenUpperCaseRegister1)
  TEST_METHOD(CodeGenVcmp)
  TEST_METHOD(CodeGenVecBitCast)
  TEST_METHOD(CodeGenVec_Comp_Arg)
  TEST_METHOD(CodeGenVecCmpCond)
  TEST_METHOD(CodeGenVecTrunc)
  TEST_METHOD(CodeGenWave)
  TEST_METHOD(CodeGenWaveNoOpt)
  TEST_METHOD(CodeGenWriteMaskBuf)
  TEST_METHOD(CodeGenWriteMaskBuf2)
  TEST_METHOD(CodeGenWriteMaskBuf3)
  TEST_METHOD(CodeGenWriteMaskBuf4)
  TEST_METHOD(CodeGenWriteToInput)
  TEST_METHOD(CodeGenWriteToInput2)
  TEST_METHOD(CodeGenWriteToInput3)
  TEST_METHOD(CodeGenWriteToInput4)

  TEST_METHOD(CodeGenAttributes_Mod)
  TEST_METHOD(CodeGenConst_Exprb_Mod)
  TEST_METHOD(CodeGenConst_Expr_Mod)
  TEST_METHOD(CodeGenFunctions_Mod)
  TEST_METHOD(CodeGenImplicit_Casts_Mod)
  TEST_METHOD(CodeGenIndexing_Operator_Mod)
  TEST_METHOD(CodeGenIntrinsic_Examples_Mod)
  TEST_METHOD(CodeGenLiterals_Mod)
  TEST_METHOD(CodeGenLiterals_Exact_Precision_Mod)
  TEST_METHOD(CodeGenMatrix_Assignments_Mod)
  TEST_METHOD(CodeGenMatrix_Syntax_Mod)
  //TEST_METHOD(CodeGenMore_Operators_Mod)
  //TEST_METHOD(CodeGenObject_Operators_Mod)
  TEST_METHOD(CodeGenPackreg_Mod)
  TEST_METHOD(CodeGenParentMethod)
  TEST_METHOD(CodeGenParameter_Types)
  TEST_METHOD(CodeGenScalar_Assignments_Mod)
  TEST_METHOD(CodeGenScalar_Operators_Assign_Mod)
  TEST_METHOD(CodeGenScalar_Operators_Mod)
  TEST_METHOD(CodeGenSemantics_Mod)
  //TEST_METHOD(CodeGenSpec_Mod)
  TEST_METHOD(CodeGenString_Mod)
  TEST_METHOD(CodeGenStruct_Assignments_Mod)
  TEST_METHOD(CodeGenStruct_AssignmentsFull_Mod)
  TEST_METHOD(CodeGenTemplate_Checks_Mod)
  TEST_METHOD(CodeGenToinclude2_Mod)
  TEST_METHOD(CodeGenTypemods_Syntax_Mod)
  TEST_METHOD(CodeGenTypedBufferHalf)
  TEST_METHOD(CodeGenTypedefNewType)
  TEST_METHOD(CodeGenVarmods_Syntax_Mod)
  TEST_METHOD(CodeGenVector_Assignments_Mod)
  TEST_METHOD(CodeGenVector_Syntax_Mix_Mod)
  TEST_METHOD(CodeGenVector_Syntax_Mod)
  TEST_METHOD(CodeGenBasicHLSL11_PS)
  TEST_METHOD(CodeGenBasicHLSL11_PS2)
  TEST_METHOD(CodeGenBasicHLSL11_PS3)
  TEST_METHOD(CodeGenBasicHLSL11_VS)
  TEST_METHOD(CodeGenBasicHLSL11_VS2)
  TEST_METHOD(CodeGenVecIndexingInput)
  TEST_METHOD(CodeGenVecMulMat)
  TEST_METHOD(CodeGenVecArrayParam)
  TEST_METHOD(CodeGenBindings1)
  TEST_METHOD(CodeGenBindings2)
  TEST_METHOD(CodeGenBindings3)
  TEST_METHOD(CodeGenResCopy)
  TEST_METHOD(CodeGenResourceParam)
  TEST_METHOD(CodeGenResourceInCB)
  TEST_METHOD(CodeGenResourceInCB2)
  TEST_METHOD(CodeGenResourceInCB3)
  TEST_METHOD(CodeGenResourceInCB4)
  TEST_METHOD(CodeGenResourceInCBV)
  TEST_METHOD(CodeGenResourceInCBV2)
  TEST_METHOD(CodeGenResourceInStruct)
  TEST_METHOD(CodeGenResourceInStruct2)
  TEST_METHOD(CodeGenResourceInStruct3)
  TEST_METHOD(CodeGenResourceInTB)
  TEST_METHOD(CodeGenResourceInTB2)
  TEST_METHOD(CodeGenResourceInTBV)
  TEST_METHOD(CodeGenResourceInTBV2)
  TEST_METHOD(CodeGenResourceArrayParam)
  TEST_METHOD(CodeGenResPhi)
  TEST_METHOD(CodeGenResPhi2)
  TEST_METHOD(CodeGenRootSigEntry)
  TEST_METHOD(CodeGenRootSigProfile)
  TEST_METHOD(CodeGenRootSigProfile2)
  TEST_METHOD(CodeGenRootSigProfile3)
  TEST_METHOD(CodeGenRootSigProfile4)
  TEST_METHOD(CodeGenRootSigProfile5)
  TEST_METHOD(CodeGenRootSigDefine1)
  TEST_METHOD(CodeGenRootSigDefine2)
  TEST_METHOD(CodeGenRootSigDefine3)
  TEST_METHOD(CodeGenRootSigDefine4)
  TEST_METHOD(CodeGenRootSigDefine5)
  TEST_METHOD(CodeGenRootSigDefine6)
  TEST_METHOD(CodeGenRootSigDefine7)
  TEST_METHOD(CodeGenRootSigDefine8)
  TEST_METHOD(CodeGenRootSigDefine9)
  TEST_METHOD(CodeGenRootSigDefine10)
  TEST_METHOD(CodeGenRootSigDefine11)
  TEST_METHOD(CodeGenCBufferStruct)
  TEST_METHOD(CodeGenCBufferStructArray)
  TEST_METHOD(CodeGenPatchLength)
  TEST_METHOD(PreprocessWhenValidThenOK)
  TEST_METHOD(PreprocessWhenExpandTokenPastingOperandThenAccept)
  TEST_METHOD(WhenSigMismatchPCFunctionThenFail)

  // Dx11 Sample
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Blurx_Ps)
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Blury_Ps)
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Vs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hdecode)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Encodeblockcs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Trymodeg10Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Trymodele10Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Decode)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Encodeblockcs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode02Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode137Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode456Cs)
  TEST_METHOD(CodeGenDX11Sample_Brightpassandhorizfiltercs)
  TEST_METHOD(CodeGenDX11Sample_Computeshadersort11)
  TEST_METHOD(CodeGenDX11Sample_Computeshadersort11_Matrixtranspose)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Sm_Vs)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Dumptotexture)
  TEST_METHOD(CodeGenDX11Sample_Filtercs_Horz)
  TEST_METHOD(CodeGenDX11Sample_Filtercs_Vertical)
  TEST_METHOD(CodeGenDX11Sample_Finalpass_Cpu_Ps)
  TEST_METHOD(CodeGenDX11Sample_Finalpass_Ps)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Buildgridcs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Buildgridindicescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Cleargridindicescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Grid)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Shared)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Simple)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Grid)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Shared)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Simple)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Integratecs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Rearrangeparticlescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidrender_Gs)
  TEST_METHOD(CodeGenDX11Sample_Fluidrender_Vs)
  TEST_METHOD(CodeGenDX11Sample_Nbodygravitycs11)
  TEST_METHOD(CodeGenDX11Sample_Oit_Createprefixsum_Pass0_Cs)
  TEST_METHOD(CodeGenDX11Sample_Oit_Createprefixsum_Pass1_Cs)
  TEST_METHOD(CodeGenDX11Sample_Oit_Fragmentcountps)
  TEST_METHOD(CodeGenDX11Sample_Oit_Ps)
  TEST_METHOD(CodeGenDX11Sample_Oit_Sortandrendercs)
  TEST_METHOD(CodeGenDX11Sample_Particledraw_Gs)
  TEST_METHOD(CodeGenDX11Sample_Particledraw_Vs)
  TEST_METHOD(CodeGenDX11Sample_Particle_Gs)
  TEST_METHOD(CodeGenDX11Sample_Particle_Ps)
  TEST_METHOD(CodeGenDX11Sample_Particle_Vs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Pom_Ps)
  TEST_METHOD(CodeGenDX11Sample_Pom_Vs)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Bloomps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale2X2_Lumps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale3X3Ps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale3X3_Brightpassps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Finalpassps)
  TEST_METHOD(CodeGenDX11Sample_Reduceto1Dcs)
  TEST_METHOD(CodeGenDX11Sample_Reducetosinglecs)
  TEST_METHOD(CodeGenDX11Sample_Rendervariancesceneps)
  TEST_METHOD(CodeGenDX11Sample_Rendervs)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Ds)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Hs)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Ps)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Bezierevalds)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Meshskinningvs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Patchskinningvs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Smoothps)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Subdtobezierhs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Subdtobezierhs4444)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Edgefactorcs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Numverticesindicescs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Scatteridcs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Tessellateindicescs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Tessellateverticescs)

  // Dx12 Sample
  TEST_METHOD(CodeGenSamplesD12_DynamicIndexing_PS)
  TEST_METHOD(CodeGenSamplesD12_ExecuteIndirect_CS)
  TEST_METHOD(CodeGenSamplesD12_MultiThreading_VS)
  TEST_METHOD(CodeGenSamplesD12_MultiThreading_PS)
  TEST_METHOD(CodeGenSamplesD12_NBodyGravity_CS)

  // Dx12 samples/MiniEngine.
  TEST_METHOD(CodeGenDx12MiniEngineAdaptexposurecs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsampleblendoutcs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplecs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplepreminblendoutcs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplepremincs)
  TEST_METHOD(CodeGenDx12MiniEngineAopreparedepthbuffers1Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAopreparedepthbuffers2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAorender1Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAorender2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineApplybloomcs)
  TEST_METHOD(CodeGenDx12MiniEngineAveragelumacs)
  TEST_METHOD(CodeGenDx12MiniEngineBicubichorizontalupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicupsamplegammaps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicverticalupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBilinearupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBloomextractanddownsamplehdrcs)
  TEST_METHOD(CodeGenDx12MiniEngineBloomextractanddownsampleldrcs)
  TEST_METHOD(CodeGenDx12MiniEngineBlurcs)
  TEST_METHOD(CodeGenDx12MiniEngineBuffercopyps)
  TEST_METHOD(CodeGenDx12MiniEngineCameramotionblurprepasscs)
  TEST_METHOD(CodeGenDx12MiniEngineCameramotionblurprepasslinearzcs)
  TEST_METHOD(CodeGenDx12MiniEngineCameravelocitycs)
  TEST_METHOD(CodeGenDx12MiniEngineConvertldrtodisplayaltps)
  TEST_METHOD(CodeGenDx12MiniEngineConvertldrtodisplayps)
  TEST_METHOD(CodeGenDx12MiniEngineDebugdrawhistogramcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugluminancehdrcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugluminanceldrcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugssaocs)
  TEST_METHOD(CodeGenDx12MiniEngineDepthviewerps)
  TEST_METHOD(CodeGenDx12MiniEngineDepthviewervs)
  TEST_METHOD(CodeGenDx12MiniEngineDownsamplebloomallcs)
  TEST_METHOD(CodeGenDx12MiniEngineDownsamplebloomcs)
  TEST_METHOD(CodeGenDx12MiniEngineExtractlumacs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass1_Luma_Cs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass1_Rgb_Cs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Hcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Hdebugcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Vcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Vdebugcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaaresolveworkqueuecs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratehistogramcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammacs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddxcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddycs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddxcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddycs)
  TEST_METHOD(CodeGenDx12MiniEngineLinearizedepthcs)
  TEST_METHOD(CodeGenDx12MiniEngineMagnifypixelsps)
  TEST_METHOD(CodeGenDx12MiniEngineModelviewerps)
  TEST_METHOD(CodeGenDx12MiniEngineModelviewervs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurfinalpasscs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurfinalpasstemporalcs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurprepasscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlebincullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticledepthboundscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticledispatchindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlefinaldispatchindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleinnersortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlelargebincullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleoutersortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlepresortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleps)
  TEST_METHOD(CodeGenDx12MiniEngineParticlesortindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlespawncs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilecullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilecullingcs_fail_unroll)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerendercs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastdynamiccs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastlowrescs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderslowdynamiccs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderslowlowrescs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleupdatecs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlevs)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphbackgroundvs)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphps)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphvs)
  TEST_METHOD(CodeGenDx12MiniEngineScreenquadvs)
  TEST_METHOD(CodeGenDx12MiniEngineSharpeningupsamplegammaps)
  TEST_METHOD(CodeGenDx12MiniEngineSharpeningupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineTemporalblendcs)
  TEST_METHOD(CodeGenDx12MiniEngineTextantialiasps)
  TEST_METHOD(CodeGenDx12MiniEngineTextshadowps)
  TEST_METHOD(CodeGenDx12MiniEngineTextvs)
  TEST_METHOD(CodeGenDx12MiniEngineTonemap2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineTonemapcs)
  TEST_METHOD(CodeGenDx12MiniEngineUpsampleandblurcs)
  TEST_METHOD(DxilGen_StoreOutput)
  TEST_METHOD(ConstantFolding)
  TEST_METHOD(HoistConstantArray)
  TEST_METHOD(VecElemConstEval)
  TEST_METHOD(ViewID)
  TEST_METHOD(SubobjectCodeGenErrors)
  TEST_METHOD(ShaderCompatSuite)
  TEST_METHOD(Unroll)
  TEST_METHOD(QuickTest)
  TEST_METHOD(QuickLlTest)
  BEGIN_TEST_METHOD(SingleFileCheckTest)
    TEST_METHOD_PROPERTY(L"Ignore", L"true")
  END_TEST_METHOD()

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned(data, size, codePage,
                                                  ppBlob));
  }

  void CreateBlobFromFile(LPCWSTR name, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    const std::wstring path = hlsl_test::GetPathToHlslDataFile(name);
    IFT(library->CreateBlobFromFile(path.c_str(), nullptr, ppBlob));
  }

  void CreateBlobFromText(_In_z_ const char *pText,
                          _Outptr_ IDxcBlobEncoding **ppBlob) {
    CreateBlobPinned(pText, strlen(pText), CP_UTF8, ppBlob);
  }

  HRESULT CreateCompiler(IDxcCompiler **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

#ifdef _WIN32 // No ContainerBuilder support yet
  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }
#endif

  template <typename T, typename TDefault, typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
                    TDefault defaultValue, LPCWSTR valueLabel,
                    HRESULT (__stdcall TIface::*pFn)(T *)) {
    T value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value != defaultValue) {
      o << L", " << valueLabel << L": " << value;
    }
  }
#ifdef _WIN32 // exclude dia stuff
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(BSTR *)) {
    CComBSTR value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.Length()) {
      o << L", " << valueLabel << L": " << (LPCWSTR)value;
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(VARIANT *)) {
    CComVariant value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.vt != VT_NULL && value.vt != VT_EMPTY) {
      if (SUCCEEDED(value.ChangeType(VT_BSTR))) {
        o << L", " << valueLabel << L": " << (LPCWSTR)value.bstrVal;
      }
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(IDiaSymbol **)) {
    CComPtr<IDiaSymbol> value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.p != nullptr) {
      DWORD symId;
      value->get_symIndexId(&symId);
      o << L", " << valueLabel << L": id=" << symId;
    }
  }

  std::wstring GetDebugInfoAsText(_In_ IDiaDataSource* pDataSource) {
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaTable> pTable;
    CComPtr<IDiaEnumTables> pEnumTables;
    std::wstringstream o;

    VERIFY_SUCCEEDED(pDataSource->openSession(&pSession));
    VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));
    LONG count;
    VERIFY_SUCCEEDED(pEnumTables->get_Count(&count));
    for (LONG i = 0; i < count; ++i) {
      pTable.Release();
      ULONG fetched;
      VERIFY_SUCCEEDED(pEnumTables->Next(1, &pTable, &fetched));
      VERIFY_ARE_EQUAL(fetched, 1);
      CComBSTR tableName;
      VERIFY_SUCCEEDED(pTable->get_name(&tableName));
      o << L"Table: " << (LPWSTR)tableName << std::endl;
      LONG rowCount;
      IFT(pTable->get_Count(&rowCount));
      o << L" Row count: " << rowCount << std::endl;

      for (LONG rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
        CComPtr<IUnknown> item;
        o << L'#' << rowIndex;
        IFT(pTable->Item(rowIndex, &item));
        CComPtr<IDiaSymbol> pSymbol;
        if (SUCCEEDED(item.QueryInterface(&pSymbol))) {
          DWORD symTag;
          DWORD dataKind;
          DWORD locationType;
          DWORD registerId;
          pSymbol->get_symTag(&symTag);
          pSymbol->get_dataKind(&dataKind);
          pSymbol->get_locationType(&locationType);
          pSymbol->get_registerId(&registerId);
          //pSymbol->get_value(&value);

          WriteIfValue(pSymbol.p, o, 0, L"symIndexId", &IDiaSymbol::get_symIndexId);
          o << L", " << SymTagEnumText[symTag];
          if (dataKind != 0) o << L", " << DataKindText[dataKind];
          WriteIfValue(pSymbol.p, o, L"name", &IDiaSymbol::get_name);
          WriteIfValue(pSymbol.p, o, L"lexicalParent", &IDiaSymbol::get_lexicalParent);
          WriteIfValue(pSymbol.p, o, L"type", &IDiaSymbol::get_type);
          WriteIfValue(pSymbol.p, o, 0, L"slot", &IDiaSymbol::get_slot);
          WriteIfValue(pSymbol.p, o, 0, L"platform", &IDiaSymbol::get_platform);
          WriteIfValue(pSymbol.p, o, 0, L"language", &IDiaSymbol::get_language);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMajor", &IDiaSymbol::get_frontEndMajor);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMinor", &IDiaSymbol::get_frontEndMinor);
          WriteIfValue(pSymbol.p, o, 0, L"token", &IDiaSymbol::get_token);
          WriteIfValue(pSymbol.p, o,    L"value", &IDiaSymbol::get_value);
          WriteIfValue(pSymbol.p, o, 0, L"code", &IDiaSymbol::get_code);
          WriteIfValue(pSymbol.p, o, 0, L"function", &IDiaSymbol::get_function);
          WriteIfValue(pSymbol.p, o, 0, L"udtKind", &IDiaSymbol::get_udtKind);
          WriteIfValue(pSymbol.p, o, 0, L"hasDebugInfo", &IDiaSymbol::get_hasDebugInfo);
          WriteIfValue(pSymbol.p, o,    L"compilerName", &IDiaSymbol::get_compilerName);
          WriteIfValue(pSymbol.p, o, 0, L"isLocationControlFlowDependent", &IDiaSymbol::get_isLocationControlFlowDependent);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfRows", &IDiaSymbol::get_numberOfRows);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfColumns", &IDiaSymbol::get_numberOfColumns);
          WriteIfValue(pSymbol.p, o, 0, L"length", &IDiaSymbol::get_length);
          WriteIfValue(pSymbol.p, o, 0, L"isMatrixRowMajor", &IDiaSymbol::get_isMatrixRowMajor);
          WriteIfValue(pSymbol.p, o, 0, L"builtInKind", &IDiaSymbol::get_builtInKind);
          WriteIfValue(pSymbol.p, o, 0, L"textureSlot", &IDiaSymbol::get_textureSlot);
          WriteIfValue(pSymbol.p, o, 0, L"memorySpaceKind", &IDiaSymbol::get_memorySpaceKind);
          WriteIfValue(pSymbol.p, o, 0, L"isHLSLData", &IDiaSymbol::get_isHLSLData);
        }

        CComPtr<IDiaSourceFile> pSourceFile;
        if (SUCCEEDED(item.QueryInterface(&pSourceFile))) {
          WriteIfValue(pSourceFile.p, o, 0, L"uniqueId", &IDiaSourceFile::get_uniqueId);
          WriteIfValue(pSourceFile.p, o, L"fileName", &IDiaSourceFile::get_fileName);
        }

        CComPtr<IDiaLineNumber> pLineNumber;
        if (SUCCEEDED(item.QueryInterface(&pLineNumber))) {
          WriteIfValue(pLineNumber.p, o, L"compiland", &IDiaLineNumber::get_compiland);
          //WriteIfValue(pLineNumber.p, o, L"sourceFile", &IDiaLineNumber::get_sourceFile);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumber", &IDiaLineNumber::get_lineNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumberEnd", &IDiaLineNumber::get_lineNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumber", &IDiaLineNumber::get_columnNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumberEnd", &IDiaLineNumber::get_columnNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"addressSection", &IDiaLineNumber::get_addressSection);
          WriteIfValue(pLineNumber.p, o, 0, L"addressOffset", &IDiaLineNumber::get_addressOffset);
          WriteIfValue(pLineNumber.p, o, 0, L"relativeVirtualAddress", &IDiaLineNumber::get_relativeVirtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"virtualAddress", &IDiaLineNumber::get_virtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"length", &IDiaLineNumber::get_length);
          WriteIfValue(pLineNumber.p, o, 0, L"sourceFileId", &IDiaLineNumber::get_sourceFileId);
          WriteIfValue(pLineNumber.p, o, 0, L"statement", &IDiaLineNumber::get_statement);
          WriteIfValue(pLineNumber.p, o, 0, L"compilandId", &IDiaLineNumber::get_compilandId);
        }

        CComPtr<IDiaSectionContrib> pSectionContrib;
        if (SUCCEEDED(item.QueryInterface(&pSectionContrib))) {
          WriteIfValue(pSectionContrib.p, o, L"compiland", &IDiaSectionContrib::get_compiland);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressSection", &IDiaSectionContrib::get_addressSection);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressOffset", &IDiaSectionContrib::get_addressOffset);
          WriteIfValue(pSectionContrib.p, o, 0, L"relativeVirtualAddress", &IDiaSectionContrib::get_relativeVirtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"virtualAddress", &IDiaSectionContrib::get_virtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"length", &IDiaSectionContrib::get_length);
          WriteIfValue(pSectionContrib.p, o, 0, L"notPaged", &IDiaSectionContrib::get_notPaged);
          WriteIfValue(pSectionContrib.p, o, 0, L"code", &IDiaSectionContrib::get_code);
          WriteIfValue(pSectionContrib.p, o, 0, L"initializedData", &IDiaSectionContrib::get_initializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"uninitializedData", &IDiaSectionContrib::get_uninitializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"remove", &IDiaSectionContrib::get_remove);
          WriteIfValue(pSectionContrib.p, o, 0, L"comdat", &IDiaSectionContrib::get_comdat);
          WriteIfValue(pSectionContrib.p, o, 0, L"discardable", &IDiaSectionContrib::get_discardable);
          WriteIfValue(pSectionContrib.p, o, 0, L"notCached", &IDiaSectionContrib::get_notCached);
          WriteIfValue(pSectionContrib.p, o, 0, L"share", &IDiaSectionContrib::get_share);
          WriteIfValue(pSectionContrib.p, o, 0, L"execute", &IDiaSectionContrib::get_execute);
          WriteIfValue(pSectionContrib.p, o, 0, L"read", &IDiaSectionContrib::get_read);
          WriteIfValue(pSectionContrib.p, o, 0, L"write", &IDiaSectionContrib::get_write);
          WriteIfValue(pSectionContrib.p, o, 0, L"dataCrc", &IDiaSectionContrib::get_dataCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"relocationsCrc", &IDiaSectionContrib::get_relocationsCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"compilandId", &IDiaSectionContrib::get_compilandId);
        }

        CComPtr<IDiaSegment> pSegment;
        if (SUCCEEDED(item.QueryInterface(&pSegment))) {
          WriteIfValue(pSegment.p, o, 0, L"frame", &IDiaSegment::get_frame);
          WriteIfValue(pSegment.p, o, 0, L"offset", &IDiaSegment::get_offset);
          WriteIfValue(pSegment.p, o, 0, L"length", &IDiaSegment::get_length);
          WriteIfValue(pSegment.p, o, 0, L"read", &IDiaSegment::get_read);
          WriteIfValue(pSegment.p, o, 0, L"write", &IDiaSegment::get_write);
          WriteIfValue(pSegment.p, o, 0, L"execute", &IDiaSegment::get_execute);
          WriteIfValue(pSegment.p, o, 0, L"addressSection", &IDiaSegment::get_addressSection);
          WriteIfValue(pSegment.p, o, 0, L"relativeVirtualAddress", &IDiaSegment::get_relativeVirtualAddress);
          WriteIfValue(pSegment.p, o, 0, L"virtualAddress", &IDiaSegment::get_virtualAddress);
        }

        CComPtr<IDiaInjectedSource> pInjectedSource;
        if (SUCCEEDED(item.QueryInterface(&pInjectedSource))) {
          WriteIfValue(pInjectedSource.p, o, 0, L"crc", &IDiaInjectedSource::get_crc);
          WriteIfValue(pInjectedSource.p, o, 0, L"length", &IDiaInjectedSource::get_length);
          WriteIfValue(pInjectedSource.p, o, L"filename", &IDiaInjectedSource::get_filename);
          WriteIfValue(pInjectedSource.p, o, L"objectFilename", &IDiaInjectedSource::get_objectFilename);
          WriteIfValue(pInjectedSource.p, o, L"virtualFilename", &IDiaInjectedSource::get_virtualFilename);
          WriteIfValue(pInjectedSource.p, o, 0, L"sourceCompression", &IDiaInjectedSource::get_sourceCompression);
          // get_source is also available
        }

        CComPtr<IDiaFrameData> pFrameData;
        if (SUCCEEDED(item.QueryInterface(&pFrameData))) {
        }

        o << std::endl;
      }
    }

    return o.str();
  }
  std::wstring GetDebugFileContent(_In_ IDiaDataSource *pDataSource) {
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaTable> pTable;

    CComPtr<IDiaTable> pSourcesTable;

    CComPtr<IDiaEnumTables> pEnumTables;
    std::wstringstream o;

    VERIFY_SUCCEEDED(pDataSource->openSession(&pSession));
    VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));

    ULONG fetched = 0;
    while (pEnumTables->Next(1, &pTable, &fetched) == S_OK && fetched == 1) {
      CComBSTR name;
      IFT(pTable->get_name(&name));

      if (wcscmp(name, L"SourceFiles") == 0) {
        pSourcesTable = pTable.Detach();
        continue;
      }

      pTable.Release();
    }

    if (!pSourcesTable) {
      return L"cannot find source";
    }

    // Get source file contents.
    // NOTE: "SourceFiles" has the root file first while "InjectedSources" is in
    // alphabetical order.
    //       It is important to keep the root file first for recompilation, so
    //       iterate "SourceFiles" and look up the corresponding injected
    //       source.
    LONG count;
    IFT(pSourcesTable->get_Count(&count));

    CComPtr<IDiaSourceFile> pSourceFile;
    CComBSTR pName;
    CComPtr<IUnknown> pSymbolUnk;
    CComPtr<IDiaEnumInjectedSources> pEnumInjectedSources;
    CComPtr<IDiaInjectedSource> pInjectedSource;
    std::wstring sourceText, sourceFilename;

    while (SUCCEEDED(pSourcesTable->Next(1, &pSymbolUnk, &fetched)) &&
           fetched == 1) {
      sourceText = sourceFilename = L"";

      IFT(pSymbolUnk->QueryInterface(&pSourceFile));
      IFT(pSourceFile->get_fileName(&pName));

      IFT(pSession->findInjectedSource(pName, &pEnumInjectedSources));

      if (SUCCEEDED(pEnumInjectedSources->get_Count(&count)) && count == 1) {
        IFT(pEnumInjectedSources->Item(0, &pInjectedSource));

        DWORD cbData = 0;
        std::string tempString;
        CComBSTR bstr;
        IFT(pInjectedSource->get_filename(&bstr));
        IFT(pInjectedSource->get_source(0, &cbData, nullptr));

        tempString.resize(cbData);
        IFT(pInjectedSource->get_source(
            cbData, &cbData, reinterpret_cast<BYTE *>(&tempString[0])));

        CA2W tempWString(tempString.data());
        o << tempWString.m_psz;
      }
      pSymbolUnk.Release();
    }

    return o.str();
  }

  struct LineNumber { DWORD line; DWORD rva; };
  std::vector<LineNumber> ReadLineNumbers(IDiaEnumLineNumbers *pEnumLineNumbers) {
    std::vector<LineNumber> lines;
    CComPtr<IDiaLineNumber> pLineNumber;
    DWORD lineCount;
    while (SUCCEEDED(pEnumLineNumbers->Next(1, &pLineNumber, &lineCount)) && lineCount == 1)
    {
      DWORD line;
      DWORD rva;
      VERIFY_SUCCEEDED(pLineNumber->get_lineNumber(&line));
      VERIFY_SUCCEEDED(pLineNumber->get_relativeVirtualAddress(&rva));
      lines.push_back({ line, rva });
      pLineNumber.Release();
    }
    return lines;
  }
#endif //  _WIN32 - exclude dia stuff
 
  std::string GetOption(std::string &cmd, char *opt) {
    std::string option = cmd.substr(cmd.find(opt));
    option = option.substr(option.find_first_of(' '));
    option = option.substr(option.find_first_not_of(' '));
    return option.substr(0, option.find_first_of(' '));
  }

  void CodeGenTest(LPCWSTR name) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromFile(name, &pSource);

    std::string cmdLine = GetFirstLine(name);

    llvm::StringRef argsRef = cmdLine;
    llvm::SmallVector<llvm::StringRef, 8> splitArgs;
    argsRef.split(splitArgs, " ");
    hlsl::options::MainArgs argStrings(splitArgs);
    std::string errorString;
    llvm::raw_string_ostream errorStream(errorString);
    hlsl::options::DxcOpts opts;
    IFT(ReadDxcOpts(hlsl::options::getHlslOptTable(), /*flagsToInclude*/ 0,
                    argStrings, opts, errorStream));
    std::wstring entry =
        Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
    std::wstring profile =
        Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());

    std::vector<std::wstring> argLists;
    CopyArgsToWStrings(opts.Args, hlsl::options::CoreOption, argLists);

    std::vector<LPCWSTR> args;
    args.reserve(argLists.size());
    for (const std::wstring &a : argLists)
      args.push_back(a.data());

    VERIFY_SUCCEEDED(pCompiler->Compile(
        pSource, name, entry.c_str(), profile.c_str(), args.data(), args.size(),
        opts.Defines.data(), opts.Defines.size(), nullptr, &pResult));
    VERIFY_IS_NOT_NULL(pResult, L"Failed to compile - pResult NULL");
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErr;
      IFT(pResult->GetErrorBuffer(&pErr));
      std::string errString(BlobToUtf8(pErr));
      CA2W errStringW(errString.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(L"Failed to compile - errors follow");
      WEX::Logging::Log::Comment(errStringW);
    }
    VERIFY_SUCCEEDED(result);

    CComPtr<IDxcBlob> pProgram;
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
    if (opts.IsRootSignatureProfile())
      return;

    CComPtr<IDxcBlobEncoding> pDisassembleBlob;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

    std::string disassembleString(BlobToUtf8(pDisassembleBlob));
    VERIFY_ARE_NOT_EQUAL(0U, disassembleString.size());
  }

  void CodeGenTestCheckFullPath(LPCWSTR fullPath) {
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath);
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CodeGenTestCheck(LPCWSTR name) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    CodeGenTestCheckFullPath(fullPath.c_str());
  }

  void CodeGenTestCheckBatch(LPCWSTR name, unsigned option) {
    WEX::Logging::Log::StartGroup(name);

    CodeGenTestCheckFullPath(name);

    WEX::Logging::Log::EndGroup(name);
  }

  void CodeGenTestCheckBatchDir(std::wstring suitePath) {
    using namespace llvm;
    using namespace WEX::TestExecution;

    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    CW2A pUtf8Filename(suitePath.c_str());
    if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
      suitePath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str());
    }

    CW2A utf8SuitePath(suitePath.c_str());

    std::error_code EC;
    llvm::SmallString<128> DirNative;
    llvm::sys::path::native(utf8SuitePath.m_psz, DirNative);
    for (llvm::sys::fs::recursive_directory_iterator Dir(DirNative, EC), DirEnd;
         Dir != DirEnd && !EC; Dir.increment(EC)) {
      // Check whether this entry has an extension typically associated with
      // headers.
      if (!llvm::StringSwitch<bool>(llvm::sys::path::extension(Dir->path()))
               .Cases(".hlsl", ".hlsl", true)
		       .Cases(".ll", ".ll", true)
               .Default(false))
        continue;
      StringRef filename = Dir->path();
      CA2W wRelPath(filename.data());
      CodeGenTestCheckBatch(wRelPath.m_psz, 0);
    }
  }

  std::string VerifyCompileFailed(LPCSTR pText, LPCWSTR pTargetProfile, LPCSTR pErrorMsg) {
    return VerifyCompileFailed(pText, pTargetProfile, pErrorMsg, L"main");
  }

  std::string VerifyCompileFailed(LPCSTR pText, LPCWSTR pTargetProfile, LPCSTR pErrorMsg, LPCWSTR pEntryPoint) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlobEncoding> pErrors;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(pText, &pSource);

    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
      pTargetProfile, nullptr, 0, nullptr, 0, nullptr, &pResult));

    HRESULT status;
    VERIFY_SUCCEEDED(pResult->GetStatus(&status));
    VERIFY_FAILED(status);
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    if (pErrorMsg && *pErrorMsg) {
      CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
    }
    return BlobToUtf8(pErrors);
  }

  void VerifyOperationSucceeded(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
      CA2W errorsWide(BlobToUtf8(pErrors).c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(errorsWide);
    }
    VERIFY_SUCCEEDED(result);
  }

  std::string VerifyOperationFailed(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    VERIFY_FAILED(result);
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    return BlobToUtf8(pErrors);
  }

#ifdef _WIN32 // - exclude dia stuff
  HRESULT CreateDiaSourceForCompile(const char *hlsl, IDiaDataSource **ppDiaSource)
  {
    if (!ppDiaSource)
      return E_POINTER;

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pProgram;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(hlsl, &pSource);
    LPCWSTR args[] = { L"/Zi" };
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      L"ps_6_0", args, _countof(args), nullptr, 0, nullptr, &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

    // Disassemble the compiled (stripped) program.
    {
      CComPtr<IDxcBlobEncoding> pDisassembly;
      VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
      std::string disText = BlobToUtf8(pDisassembly);
      CA2W disTextW(disText.c_str(), CP_UTF8);
      //WEX::Logging::Log::Comment(disTextW);
    }

    // CONSIDER: have the dia data source look for the part if passed a whole container.
    CComPtr<IDiaDataSource> pDiaSource;
    CComPtr<IStream> pProgramStream;
    CComPtr<IDxcLibrary> pLib;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    const hlsl::DxilContainerHeader *pContainer = hlsl::IsDxilContainerLike(
        pProgram->GetBufferPointer(), pProgram->GetBufferSize());
    VERIFY_IS_NOT_NULL(pContainer);
    hlsl::DxilPartIterator partIter =
        std::find_if(hlsl::begin(pContainer), hlsl::end(pContainer),
                     hlsl::DxilPartIsType(hlsl::DFCC_ShaderDebugInfoDXIL));
    const hlsl::DxilProgramHeader *pProgramHeader =
        (const hlsl::DxilProgramHeader *)hlsl::GetDxilPartData(*partIter);
    uint32_t bitcodeLength;
    const char *pBitcode;
    CComPtr<IDxcBlob> pProgramPdb;
    hlsl::GetDxilProgramBitcode(pProgramHeader, &pBitcode, &bitcodeLength);
    VERIFY_SUCCEEDED(pLib->CreateBlobFromBlob(
        pProgram, pBitcode - (char *)pProgram->GetBufferPointer(), bitcodeLength,
        &pProgramPdb));

    // Disassemble the program with debug information.
    {
      CComPtr<IDxcBlobEncoding> pDbgDisassembly;
      VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgramPdb, &pDbgDisassembly));
      std::string disText = BlobToUtf8(pDbgDisassembly);
      CA2W disTextW(disText.c_str(), CP_UTF8);
      //WEX::Logging::Log::Comment(disTextW);
    }

    // Create a short text dump of debug information.
    VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pProgramPdb, &pProgramStream));
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
    VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pProgramStream));
    *ppDiaSource = pDiaSource.Detach();
    return S_OK;
  }
#endif // _WIN32 - exclude dia stuff
};

// Useful for debugging.
#if SUPPORT_FXC_PDB
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
HRESULT GetBlobPdb(IDxcBlob *pBlob, IDxcBlob **ppDebugInfo) {
  return D3DGetBlobPart(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
    D3D_BLOB_PDB, 0, (ID3DBlob **)ppDebugInfo);
}

std::string FourCCStr(uint32_t val) {
  std::stringstream o;
  char c[5];
  c[0] = val & 0xFF;
  c[1] = (val & 0xFF00) >> 8;
  c[2] = (val & 0xFF0000) >> 16;
  c[3] = (val & 0xFF000000) >> 24;
  c[4] = '\0';
  o << c << " (" << std::hex << val << std::dec << ")";
  return o.str();
}
std::string DumpParts(IDxcBlob *pBlob) {
  std::stringstream o;

  hlsl::DxilContainerHeader *pContainer = (hlsl::DxilContainerHeader *)pBlob->GetBufferPointer();
  o << "Container:" << std::endl
    << " Size: " << pContainer->ContainerSizeInBytes << std::endl
    << " FourCC: " << FourCCStr(pContainer->HeaderFourCC) << std::endl
    << " Part count: " << pContainer->PartCount << std::endl;
  for (uint32_t i = 0; i < pContainer->PartCount; ++i) {
    hlsl::DxilPartHeader *pPart = hlsl::GetDxilContainerPart(pContainer, i);
    o << "Part " << i << std::endl
      << " FourCC: " << FourCCStr(pPart->PartFourCC) << std::endl
      << " Size: " << pPart->PartSize << std::endl;
  }
  return o.str();
}

HRESULT CreateDiaSourceFromDxbcBlob(IDxcLibrary *pLib, IDxcBlob *pDxbcBlob,
                                    IDiaDataSource **ppDiaSource) {
  HRESULT hr = S_OK;
  CComPtr<IDxcBlob> pdbBlob;
  CComPtr<IStream> pPdbStream;
  CComPtr<IDiaDataSource> pDiaSource;
  IFR(GetBlobPdb(pDxbcBlob, &pdbBlob));
  IFR(pLib->CreateStreamFromBlobReadOnly(pdbBlob, &pPdbStream));
  IFR(CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER,
                       __uuidof(IDiaDataSource), (void **)&pDiaSource));
  IFR(pDiaSource->loadDataFromIStream(pPdbStream));
  *ppDiaSource = pDiaSource.Detach();
  return hr;
}
#endif

bool CompilerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

#if _WIN32 // - exclude dia stuff
TEST_F(CompilerTest, CompileWhenDebugThenDIPresent) {
  // BUG: the first test written was of this form:
  // float4 local = 0; return local;
  //
  // However we get no numbers because of the _wrapper form
  // that exports the zero initialization from main into
  // a global can't be attributed to any particular location
  // within main, and everything in main is eventually folded away.
  //
  // Making the function do a bit more work by calling an intrinsic
  // helps this case.
  CComPtr<IDiaDataSource> pDiaSource;
  VERIFY_SUCCEEDED(CreateDiaSourceForCompile(
    "float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
    "  float4 local = abs(pos);\r\n"
    "  return local;\r\n"
    "}", &pDiaSource));
  std::wstring diaDump = GetDebugInfoAsText(pDiaSource).c_str();
  //WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());

  // Very basic tests - we have basic symbols, line numbers, and files with sources.
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"symIndexId: 5, CompilandEnv, name: hlslTarget, value: ps_6_0"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"lineNumber: 2"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"length: 99, filename: source.hlsl"));
  std::wstring diaFileContent = GetDebugFileContent(pDiaSource).c_str();
  VERIFY_IS_NOT_NULL(wcsstr(diaFileContent.c_str(), L"loat4 main(float4 pos : SV_Position) : SV_Target"));
#if SUPPORT_FXC_PDB
  // Now, fake it by loading from a .pdb!
  VERIFY_SUCCEEDED(CoInitializeEx(0, COINITBASE_MULTITHREADED));
  const wchar_t path[] = L"path-to-fxc-blob.bin";
  pDiaSource.Release();
  pProgramStream.Release();
  CComPtr<IDxcBlobEncoding> fxcBlob;
  CComPtr<IDxcBlob> pdbBlob;
  VERIFY_SUCCEEDED(pLib->CreateBlobFromFile(path, nullptr, &fxcBlob));
  std::string s = DumpParts(fxcBlob);
  CA2W sW(s.c_str(), CP_UTF8);
  WEX::Logging::Log::Comment(sW);
  VERIFY_SUCCEEDED(CreateDiaSourceFromDxbcBlob(pLib, fxcBlob, &pDiaSource));
  WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());
#endif
}

TEST_F(CompilerTest, CompileDebugLines) {
  CComPtr<IDiaDataSource> pDiaSource;
  VERIFY_SUCCEEDED(CreateDiaSourceForCompile(
    "float main(float pos : A) : SV_Target {\r\n"
    "  float x = abs(pos);\r\n"
    "  float y = sin(pos);\r\n"
    "  float z = x + y;\r\n"
    "  return z;\r\n"
    "}", &pDiaSource));
    
  const int numExpectedRVAs = 6;

  auto verifyLines = [=](const std::vector<LineNumber> lines) {
    VERIFY_ARE_EQUAL(lines.size(), numExpectedRVAs);
    // 0: loadInput
    VERIFY_ARE_EQUAL(lines[0].rva,  0);
    VERIFY_ARE_EQUAL(lines[0].line, 1);
    // 1: abs
    VERIFY_ARE_EQUAL(lines[1].rva,  1);
    VERIFY_ARE_EQUAL(lines[1].line, 2);
    // 2: sin
    VERIFY_ARE_EQUAL(lines[2].rva,  2);
    VERIFY_ARE_EQUAL(lines[2].line, 3);
    // 3: add
    VERIFY_ARE_EQUAL(lines[3].rva,  3);
    VERIFY_ARE_EQUAL(lines[3].line, 4);
    // 4: storeOutput
    VERIFY_ARE_EQUAL(lines[4].rva,  4);
    VERIFY_ARE_EQUAL(lines[4].line, 5);
    // 5: ret
    VERIFY_ARE_EQUAL(lines[5].rva,  5);
    VERIFY_ARE_EQUAL(lines[5].line, 5);
  };
  
  CComPtr<IDiaSession> pSession;
  CComPtr<IDiaEnumLineNumbers> pEnumLineNumbers;

  // Verify lines are ok when getting one RVA at a time.
  std::vector<LineNumber> linesOneByOne;
  VERIFY_SUCCEEDED(pDiaSource->openSession(&pSession));
  for (int i = 0; i < numExpectedRVAs; ++i) {
    VERIFY_SUCCEEDED(pSession->findLinesByRVA(i, 1, &pEnumLineNumbers));
    std::vector<LineNumber> lines = ReadLineNumbers(pEnumLineNumbers);
    std::copy(lines.begin(), lines.end(), std::back_inserter(linesOneByOne));
    pEnumLineNumbers.Release();
  }
  verifyLines(linesOneByOne);

  // Verify lines are ok when getting all RVAs at once.
  std::vector<LineNumber> linesAllAtOnce;
  pEnumLineNumbers.Release();
  VERIFY_SUCCEEDED(pSession->findLinesByRVA(0, numExpectedRVAs, &pEnumLineNumbers));
  linesAllAtOnce = ReadLineNumbers(pEnumLineNumbers);
  verifyLines(linesAllAtOnce);

  // Verify lines are ok when getting all lines through enum tables.
  std::vector<LineNumber> linesFromTable;
  pEnumLineNumbers.Release();
  CComPtr<IDiaEnumTables> pTables;
  CComPtr<IDiaTable> pTable;
  VERIFY_SUCCEEDED(pSession->getEnumTables(&pTables));
  DWORD celt;
  while (SUCCEEDED(pTables->Next(1, &pTable, &celt)) && celt == 1)
  {
    if (SUCCEEDED(pTable->QueryInterface(&pEnumLineNumbers))) {
      linesFromTable = ReadLineNumbers(pEnumLineNumbers);
      break;
    }
    pTable.Release();
  }
  verifyLines(linesFromTable);
  
  // Verify lines are ok when getting by address.
  std::vector<LineNumber> linesByAddr;
  pEnumLineNumbers.Release();
  VERIFY_SUCCEEDED(pSession->findLinesByAddr(0, 0, numExpectedRVAs, &pEnumLineNumbers));
  linesByAddr = ReadLineNumbers(pEnumLineNumbers);
  verifyLines(linesByAddr);

  // Verify findFileById.
  CComPtr<IDiaSourceFile> pFile;
  VERIFY_SUCCEEDED(pSession->findFileById(0, &pFile));
  CComBSTR pName;
  VERIFY_SUCCEEDED(pFile->get_fileName(&pName));
  VERIFY_ARE_EQUAL_WSTR(pName, L"source.hlsl");
}
#endif // _WIN32 - exclude dia stuff

TEST_F(CompilerTest, CompileWhenDefinesThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[] = {{L"F4", L"float4"}};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("F4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, defines,
                                      _countof(defines), nullptr, &pResult));
}

TEST_F(CompilerTest, CompileWhenDefinesManyThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  LPCWSTR args[] = {L"/DVAL1=1",  L"/DVAL2=2",  L"/DVAL3=3",  L"/DVAL4=2",
                    L"/DVAL5=4",  L"/DNVAL1",   L"/DNVAL2",   L"/DNVAL3",
                    L"/DNVAL4",   L"/DNVAL5",   L"/DCVAL1=1", L"/DCVAL2=2",
                    L"/DCVAL3=3", L"/DCVAL4=2", L"/DCVAL5=4", L"/DCVALNONE="};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
                     "#ifndef VAL1\r\n"
                     "#error VAL1 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef NVAL5\r\n"
                     "#error NVAL5 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef CVALNONE\r\n"
                     "#error CVALNONE not defined\r\n"
                     "#endif\r\n"
                     "return 0; }",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  HRESULT compileStatus;
  VERIFY_SUCCEEDED(pResult->GetStatus(&compileStatus));
  if (FAILED(compileStatus)) {
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    OutputDebugStringA((LPCSTR)pErrors->GetBufferPointer());
  }
  VERIFY_SUCCEEDED(compileStatus);
}

TEST_F(CompilerTest, CompileWhenEmptyThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pSourceBad;
  LPCWSTR pProfile = L"ps_6_0";
  LPCWSTR pEntryPoint = L"main";

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  CreateBlobFromText("float4 main() : SV_Target { return undef; }", &pSourceBad);

  // correct version
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
                                      pProfile, nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // correct version with compilation errors
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, L"source.hlsl", pEntryPoint,
                                      pProfile, nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // null source
  VERIFY_FAILED(pCompiler->Compile(nullptr, L"source.hlsl", pEntryPoint, pProfile,
                                   nullptr, 0, nullptr, 0, nullptr, &pResult));

  // null profile
  VERIFY_FAILED(pCompiler->Compile(pSourceBad, L"source.hlsl", pEntryPoint,
                                   nullptr, nullptr, 0, nullptr, 0, nullptr,
                                   &pResult));

  // null source name succeeds
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, nullptr, pEntryPoint, pProfile,
                                   nullptr, 0, nullptr, 0, nullptr, &pResult));
  pResult.Release();

  // empty source name (as opposed to null) also suceeds
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, L"", pEntryPoint, pProfile,
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // null result
  VERIFY_FAILED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
                                   pProfile, nullptr, 0, nullptr, 0, nullptr,
                                   nullptr));
}

TEST_F(CompilerTest, CompileWhenIncorrectThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4_undefined main() : SV_Target { return 0; }",
                     &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", L"ps_6_0",
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_FAILED(result);

  CComPtr<IDxcBlobEncoding> pErrorBuffer;
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrorBuffer));
  std::string errorString(BlobToUtf8(pErrorBuffer));
  VERIFY_ARE_NOT_EQUAL(0U, errorString.size());
  // Useful for examining actual error message:
  // CA2W errorStringW(errorString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(errorStringW.m_psz);
}

TEST_F(CompilerTest, CompileWhenWorksThenDisassembleWorks) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_SUCCEEDED(result);

  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  CComPtr<IDxcBlobEncoding> pDisassembleBlob;
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

  std::string disassembleString(BlobToUtf8(pDisassembleBlob));
  VERIFY_ARE_NOT_EQUAL(0U, disassembleString.size());
  // Useful for examining disassembly:
  // CA2W disassembleStringW(disassembleString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(disassembleStringW.m_psz);
}

#ifdef _WIN32 // Container builder unsupported

TEST_F(CompilerTest, CompileWhenDebugWorksThenStripDebug) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
                     "  float4 local = abs(pos);\r\n"
                     "  return local;\r\n"
                     "}",
                     &pSource);
  LPCWSTR args[] = {L"/Zi"};

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Check if it contains debug blob
  hlsl::DxilContainerHeader *pHeader =
      (hlsl::DxilContainerHeader *)(pProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // Check debug info part does not exist after strip debug info

  CComPtr<IDxcBlob> pNewProgram;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pHeader = (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWhenWorksThenAddRemovePrivate) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
    "  return 0;\r\n"
    "}",
    &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0,
    nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Append private data blob
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));

  std::string privateTxt("private data");
  CComPtr<IDxcBlobEncoding> pPrivate;
  CreateBlobFromText(privateTxt.c_str(), &pPrivate);
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_PrivateData, pPrivate));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  CComPtr<IDxcBlob> pNewProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  hlsl::DxilContainerHeader *pContainerHeader =
    (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // compare data
  std::string privatePart((const char *)(pPartHeader + 1), privateTxt.size());
  VERIFY_IS_TRUE(strcmp(privatePart.c_str(), privateTxt.c_str()) == 0);

  // Remove private data blob
  pBuilder.Release();
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pNewProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_PrivateData));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  pNewProgram.Release();
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader =
    (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileThenAddCustomDebugName) {
  // container builders prior to 1.3 did not support adding debug name parts
  if (m_ver.SkipDxilVersion(1, 3)) return;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
    "  return 0;\r\n"
    "}",
    &pSource);

  LPCWSTR args[] = { L"/Zi", L"/Zss" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0,
    nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Append private data blob
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));

  const char pNewName[] = "MyOwnUniqueName.lld";
  //include null terminator:
  size_t nameBlobPartSize = sizeof(hlsl::DxilShaderDebugName) + _countof(pNewName);
  // round up to four-byte size:
  size_t allocatedSize = (nameBlobPartSize + 3) & ~3;
  auto pNameBlobContent = reinterpret_cast<hlsl::DxilShaderDebugName*>(malloc(allocatedSize));
  ZeroMemory(pNameBlobContent, allocatedSize); //just to make sure trailing nulls are nulls.
  pNameBlobContent->Flags = 0;
  pNameBlobContent->NameLength = _countof(pNewName) - 1; //this is not supposed to include null terminator
  memcpy(pNameBlobContent + 1, pNewName, _countof(pNewName));

  CComPtr<IDxcBlobEncoding> pDebugName;

  CreateBlobPinned(pNameBlobContent, allocatedSize, CP_UTF8, &pDebugName);


  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  // should fail since it already exists:
  VERIFY_FAILED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_ShaderDebugName, pDebugName));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugName));
  VERIFY_SUCCEEDED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_ShaderDebugName, pDebugName));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  CComPtr<IDxcBlob> pNewProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  hlsl::DxilContainerHeader *pContainerHeader =
    (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_ShaderDebugName);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // compare data
  VERIFY_IS_TRUE(memcmp(pPartHeader + 1, pNameBlobContent, allocatedSize) == 0);

  free(pNameBlobContent);

  // Remove private data blob
  pBuilder.Release();
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pNewProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugName));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  pNewProgram.Release();
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader =
    (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_ShaderDebugName);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWithRootSignatureThenStripRootSignature) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("[RootSignature(\"\")] \r\n"
                     "float4 main(float a : A) : SV_Target {\r\n"
                     "  return a;\r\n"
                     "}",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr,
                                      0, nullptr, &pResult));
  VERIFY_IS_NOT_NULL(pResult);
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  VERIFY_IS_NOT_NULL(pProgram);
  hlsl::DxilContainerHeader *pContainerHeader =
      (hlsl::DxilContainerHeader *)(pProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pContainerHeader, hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NOT_NULL(pPartHeader);
  
  // Remove root signature
  CComPtr<IDxcBlob> pNewProgram;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_RootSignature));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader = (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(pContainerHeader,
                                        hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NULL(pPartHeader);
}
#endif // Container builder unsupported

TEST_F(CompilerTest, CompileWhenIncludeThenLoadInvoked) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeThenLoadUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeAbsoluteThenLoadAbsolute) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
#ifdef _WIN32 // OS-specific root
  CreateBlobFromText(
    "#include \"C:\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);
#else
  CreateBlobFromText(
    "#include \"/helper.h\"\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);
#endif


  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific root
  VERIFY_ARE_EQUAL_WSTR(L"C:\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeLocalThenLoadRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"..\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific directory dividers
  VERIFY_ARE_EQUAL_WSTR(L"./..\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"./../helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeSystemThenLoadNotRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  LPCWSTR args[] = {
    L"-Ifoo"
  };
  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific directory dividers
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./foo\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./foo/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeSystemMissingThenLoadAttempt) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(std::string::npos, failLog.find("<angled>")); // error message should prompt to use <angled> rather than "quotes"
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./subdir/other/helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeFlagsThenIncludeUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include <helper.h>\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

#ifdef _WIN32  // OS-specific root
  LPCWSTR args[] = { L"-I\\\\server\\share" };
#else
  LPCWSTR args[] = { L"-I/server/share" };
#endif
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32  // OS-specific root
  VERIFY_ARE_EQUAL_WSTR(L"\\\\server\\share\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"/server/share/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeMissingThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"file.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  HRESULT hr;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
  VERIFY_FAILED(hr);
}

TEST_F(CompilerTest, CompileWhenIncludeHasPathThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  LPCWSTR Source = L"c:\\temp\\OddIncludes\\main.hlsl";
  LPCWSTR Args[] = { L"/I", L"c:\\temp" };
  LPCWSTR ArgsUp[] = { L"/I", L"c:\\Temp" };
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  bool useUpValues[] = { false, true };
  for (bool useUp : useUpValues) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
#if TEST_ON_DISK
    CComPtr<IDxcLibrary> pLibrary;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateIncludeHandler(&pInclude));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobFromFile(Source, nullptr, &pSource));
#else
    CComPtr<TestIncludeHandler> pInclude;
    pInclude = new TestIncludeHandler(m_dllSupport);
    pInclude->CallResults.emplace_back("// Empty");
    CreateBlobFromText("#include \"include.hlsl\"\r\n"
                       "float4 main() : SV_Target { return 0; }",
                       &pSource);
#endif

    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, Source, L"main",
      L"ps_6_0", useUp ? ArgsUp : Args, _countof(Args), nullptr, 0, pInclude, &pResult));
    HRESULT hr;
    VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
    VERIFY_SUCCEEDED(hr);
 }
}

TEST_F(CompilerTest, CompileWhenIncludeEmptyThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("#include \"empty.h\"\r\n"
                     "float4 main() : SV_Target { return 0; }",
                     &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("", CP_ACP); // An empty file would get detected as ACP code page

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./empty.h;", pInclude->GetAllFileNames().c_str());
}

static const char EmptyCompute[] = "[numthreads(8,8,1)] void main() { }";

TEST_F(CompilerTest, CompileWhenODumpThenPassConfig) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Odump" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  string passes((char *)pResultBlob->GetBufferPointer(), pResultBlob->GetBufferSize());
  VERIFY_ARE_NOT_EQUAL(string::npos, passes.find("inline"));
}

TEST_F(CompilerTest, CompileWhenVdThenProducesDxilContainer) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Vd" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(reinterpret_cast<hlsl::DxilContainerHeader *>(pResultBlob->GetBufferPointer()), pResultBlob->GetBufferSize()));
}

TEST_F(CompilerTest, CompileWhenODumpThenOptimizerMatch) {
  LPCWSTR OptLevels[] = { L"/Od", L"/O1", L"/O2" };
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOptimizer> pOptimizer;
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcValidator> pValidator;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
  for (LPCWSTR OptLevel : OptLevels) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pHighLevelBlob;
    CComPtr<IDxcBlob> pOptimizedModule;
    CComPtr<IDxcBlob> pAssembledBlob;

    // Could use EmptyCompute and cs_6_0, but there is an issue where properties
    // don't round-trip properly at high-level, so validation fails because
    // dimensions are set to zero. Workaround by using pixel shader instead.
    LPCWSTR Target = L"ps_6_0";
    CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

    LPCWSTR Args[2] = { OptLevel, L"/Odump" };

    // Get the passes for this optimization level.
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    CComPtr<IDxcBlob> pResultBlob;
    VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
    string passes((char *)pResultBlob->GetBufferPointer(), pResultBlob->GetBufferSize());

    // Get wchar_t version and prepend hlsl-hlensure, to do a split high-level/opt compilation pass.
    CA2W passesW(passes.c_str(), CP_UTF8);
    std::vector<LPCWSTR> Options;
    SplitPassList(passesW.m_psz, Options);

    // Now compile directly.
    pResult.Release();
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, 1, nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);

    // Now compile via a high-level compile followed by the optimization passes.
    pResult.Release();
    Args[_countof(Args)-1] = L"/fcgl";
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pHighLevelBlob));
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pHighLevelBlob, Options.data(),
                                              Options.size(), &pOptimizedModule,
                                              nullptr));

    string text = DisassembleProgram(m_dllSupport, pOptimizedModule);
    WEX::Logging::Log::Comment(L"Final program:");
    WEX::Logging::Log::Comment(CA2W(text.c_str()));

    // At the very least, the module should be valid.
    pResult.Release();
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pOptimizedModule, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pAssembledBlob));
    pResult.Release();
    VERIFY_SUCCEEDED(pValidator->Validate(pAssembledBlob, DxcValidatorFlags_Default, &pResult));
    VerifyOperationSucceeded(pResult);
  }
}

static const UINT CaptureStacks = 0; // Set to 1 to enable captures
static const UINT StackFrameCount = 12;

struct InstrumentedHeapMalloc : public IMalloc {
private:
  HANDLE m_Handle;        // Heap handle.
  ULONG m_RefCount = 0;   // Reference count. Used for reference leaks, not for lifetime.
  ULONG m_AllocCount = 0; // Total # of alloc and realloc requests.
  ULONG m_AllocSize = 0;  // Total # of alloc and realloc bytes.
  ULONG m_Size = 0;       // Current # of alloc'ed bytes.
  ULONG m_FailAlloc = 0;  // If nonzero, the alloc/realloc call to fail.
  // Each allocation also tracks the following information:
  // - allocation callstack
  // - deallocation callstack
  // - prior/next blocks in a list of allocated blocks
  LIST_ENTRY AllocList;
  struct PtrData {
    LIST_ENTRY Entry;
    LPVOID AllocFrames[CaptureStacks ? StackFrameCount * CaptureStacks : 1];
    LPVOID FreeFrames[CaptureStacks ? StackFrameCount * CaptureStacks : 1];
    UINT64 AllocAtCount;
    DWORD AllocFrameCount;
    DWORD FreeFrameCount;
    SIZE_T Size;
    PtrData *Self;
  };
  PtrData *DataFromPtr(void *p) {
    if (p == nullptr) return nullptr;
    PtrData *R = ((PtrData *)p) - 1;
    if (R != R->Self) {
      VERIFY_FAIL(); // p is invalid or underrun
    }
    return R;
  }
public:
  InstrumentedHeapMalloc() : m_Handle(nullptr) {
    ResetCounts();
  }
  ~InstrumentedHeapMalloc() {
    if (m_Handle)
      HeapDestroy(m_Handle);
  }
  void ResetHeap() {
    if (m_Handle) {
      HeapDestroy(m_Handle);
      m_Handle = nullptr;
    }
    m_Handle = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
  }
  ULONG GetRefCount() const { return m_RefCount; }
  ULONG GetAllocCount() const { return m_AllocCount; }
  ULONG GetAllocSize() const { return m_AllocSize; }
  ULONG GetSize() const { return m_Size; }

  void ResetCounts() {
    m_RefCount = m_AllocCount = m_AllocSize = m_Size = 0;
    AllocList.Blink = AllocList.Flink = &AllocList;
  }
  void SetFailAlloc(ULONG index) {
    m_FailAlloc = index;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return ++m_RefCount;
  }
  ULONG STDMETHODCALLTYPE Release() {
    if (m_RefCount == 0) VERIFY_FAIL();
    return --m_RefCount;
  }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
    return DoBasicQueryInterface<IMalloc>(this, iid, ppvObject);
  }
  virtual void *STDMETHODCALLTYPE Alloc(_In_ SIZE_T cb) {
    ++m_AllocCount;
    if (m_FailAlloc && m_AllocCount >= m_FailAlloc) {
      return nullptr; // breakpoint for i failure - m_FailAlloc == 1+VAL
    }
    m_AllocSize += cb;
    m_Size += cb;
    PtrData *P = (PtrData *)HeapAlloc(m_Handle, HEAP_ZERO_MEMORY, sizeof(PtrData) + cb);
    P->Entry.Flink = AllocList.Flink;
    P->Entry.Blink = &AllocList;
    AllocList.Flink->Blink = &(P->Entry);
    AllocList.Flink = &(P->Entry);
    // breakpoint for i failure on NN alloc - m_FailAlloc == 1+VAL && m_AllocCount == NN
    // breakpoint for happy path for NN alloc - m_AllocCount == NN
    P->AllocAtCount = m_AllocCount;
    if (CaptureStacks)
      P->AllocFrameCount = CaptureStackBackTrace(1, StackFrameCount, P->AllocFrames, nullptr);
    P->Size = cb;
    P->Self = P;
    return P + 1;
  }

  virtual void *STDMETHODCALLTYPE Realloc(_In_opt_ void *pv, _In_ SIZE_T cb) {
    SIZE_T priorSize = pv == nullptr ? (SIZE_T)0 : GetSize(pv);
    void *R = Alloc(cb);
    if (!R)
      return nullptr;
    SIZE_T copySize = std::min(cb, priorSize);
    memcpy(R, pv, copySize);
    Free(pv);
    return R;
  }

  virtual void STDMETHODCALLTYPE Free(_In_opt_ void *pv) {
    if (!pv)
      return;
    PtrData *P = DataFromPtr(pv);
    if (P->FreeFrameCount)
      VERIFY_FAIL(); // double-free detected
    m_Size -= P->Size;
    P->Entry.Flink->Blink = P->Entry.Blink;
    P->Entry.Blink->Flink = P->Entry.Flink;
    if (CaptureStacks)
      P->FreeFrameCount =
          CaptureStackBackTrace(1, StackFrameCount, P->FreeFrames, nullptr);
  }

  virtual SIZE_T STDMETHODCALLTYPE GetSize(
    /* [annotation][in] */
    _In_opt_ _Post_writable_byte_size_(return)  void *pv)
  {
    if (pv == nullptr) return 0;
    return DataFromPtr(pv)->Size;
  }

  virtual int STDMETHODCALLTYPE DidAlloc(
      _In_opt_ void *pv) {
    return -1; // don't know
  }

  virtual void STDMETHODCALLTYPE HeapMinimize(void) {}

  void DumpLeaks() {
    PtrData *ptr = (PtrData*)AllocList.Flink;;
    PtrData *end = (PtrData*)AllocList.Blink;;

    WEX::Logging::Log::Comment(FormatToWString(L"Leaks total size: %d", (signed int)m_Size).data());
    while (ptr != end) {
      WEX::Logging::Log::Comment(FormatToWString(L"Memory leak at 0x0%X, size %d, alloc# %d", ptr + 1, ptr->Size, ptr->AllocAtCount).data());
      ptr = (PtrData*)ptr->Entry.Flink;
    }
  }
};

TEST_F(CompilerTest, CompileWhenNoMemThenOOM) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  CComPtr<IDxcBlobEncoding> pSource;
  CreateBlobFromText(EmptyCompute, &pSource);

  InstrumentedHeapMalloc InstrMalloc;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  ULONG allocCount = 0;
  ULONG allocSize = 0;
  ULONG initialRefCount;

  InstrMalloc.ResetHeap();

  VERIFY_IS_TRUE(m_dllSupport.HasCreateWithMalloc());

  // Verify a simple object creation.
  initialRefCount = InstrMalloc.GetRefCount();
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler));
  pCompiler.Release();
  VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
  VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());
  InstrMalloc.ResetCounts();
  InstrMalloc.ResetHeap();

  // First time, run to completion and capture stats.
  initialRefCount = InstrMalloc.GetRefCount();
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  allocCount = InstrMalloc.GetAllocCount();
  allocSize = InstrMalloc.GetAllocSize();

  HRESULT hrWithMemory;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrWithMemory));
  VERIFY_SUCCEEDED(hrWithMemory);

  pCompiler.Release();
  pResult.Release();

  VERIFY_IS_TRUE(allocSize > allocCount);

  // Ensure that after all resources are released, there are no outstanding
  // allocations or references.
  //
  // First leak is in ((InstrumentedHeapMalloc::PtrData *)InstrMalloc.AllocList.Flink)
  if (InstrMalloc.GetSize() != 0) {
    WEX::Logging::Log::Comment(L"Memory leak(s) detected");
    InstrMalloc.DumpLeaks();
    VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
  }

  VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());

  // In Debug, without /D_ITERATOR_DEBUG_LEVEL=0, debug iterators will be used;
  // this causes a problem where std::string is specified as noexcept, and yet
  // a sentinel is allocated that may fail and throw.
  if (m_ver.SkipOutOfMemoryTest()) return;

  // Now, fail each allocation and make sure we get an error.
  for (ULONG i = 0; i <= allocCount; ++i) {
    // LogCommentFmt(L"alloc fail %u", i);
    bool isLast = i == allocCount;
    InstrMalloc.ResetCounts();
    InstrMalloc.ResetHeap();
    InstrMalloc.SetFailAlloc(i + 1);
    HRESULT hrOp = m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler);
    if (SUCCEEDED(hrOp)) {
      hrOp = pCompiler->Compile(pSource, L"source.hlsl", L"main", L"cs_6_0",
                                nullptr, 0, nullptr, 0, nullptr, &pResult);
      if (SUCCEEDED(hrOp)) {
        pResult->GetStatus(&hrOp);
      }
    }
    if (FAILED(hrOp)) {
      // This is true in *almost* every case. When the OOM happens during stream
      // handling, there is no specific error set; by the time it's detected,
      // it propagates as E_FAIL.
      //VERIFY_ARE_EQUAL(hrOp, E_OUTOFMEMORY);
      VERIFY_IS_TRUE(hrOp == E_OUTOFMEMORY || hrOp == E_FAIL);
    }
    if (isLast)
      VERIFY_SUCCEEDED(hrOp);
    else
      VERIFY_FAILED(hrOp);
    pCompiler.Release();
    pResult.Release();
    
    if (InstrMalloc.GetSize() != 0) {
      WEX::Logging::Log::Comment(FormatToWString(L"Memory leak(s) detected, allocCount = %d", i).data()); 
      InstrMalloc.DumpLeaks();
      VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
    }
    VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());
  }
}

TEST_F(CompilerTest, CompileWhenShaderModelMismatchAttributeThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find("attribute numthreads only valid for CS"));
}

TEST_F(CompilerTest, CompileBadHlslThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "bad hlsl", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_FAILED(status);
}

TEST_F(CompilerTest, CompileLegacyShaderModelThenFail) {
  VerifyCompileFailed(
    "float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", L"ps_5_1", nullptr);
}

TEST_F(CompilerTest, CompileWhenRecursiveAlbeitStaticTermThenFail) {
  // This shader will compile under fxc because if execution is
  // simulated statically, it does terminate. dxc changes this behavior
  // to avoid imposing the requirement on the compiler.
  const char ShaderText[] =
    "static int i = 10;\r\n"
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { if (i > 10) { i--; return f(); } else return 0; } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 VS() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderText, L"vs_6_0", "recursive functions not allowed", L"VS");
}

TEST_F(CompilerTest, CompileWhenRecursiveThenFail) {
  const char ShaderTextSimple[] =
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { return f(); } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 main() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextSimple, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextIndirect[] =
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { return f(); } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 main() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextIndirect, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextSelf[] =
    "float4 main() : SV_Position{\r\n"
    "  return main();\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextSelf, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextMissing[] =
    "float4 mainz() : SV_Position{\r\n"
    "  return 1;\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextMissing, L"vs_6_0", "missing entry point definition");
}

TEST_F(CompilerTest, CompileHlsl2015ThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2015" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_ARE_EQUAL(status, E_INVALIDARG);
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
  LPCSTR pErrorMsg = "HLSL Version 2015 is only supported for language services";
  CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
}

TEST_F(CompilerTest, CompileHlsl2016ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2016" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2017ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2017" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2018ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2018" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2019ThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2019" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_ARE_EQUAL(status, E_INVALIDARG);
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
  LPCSTR pErrorMsg = "Unknown HLSL version";
  CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
}

TEST_F(CompilerTest, CompileCBufferTBufferASTDump) {
  CodeGenTestCheck(L"ctbuf.hlsl");
}

#ifdef _WIN32 // - exclude dia stuff
TEST_F(CompilerTest, DiaLoadBadBitcodeThenFail) {
  CComPtr<IDxcBlob> pBadBitcode;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;
  CComPtr<IDxcLibrary> pLib;

  Utf8ToBlob(m_dllSupport, "badcode", &pBadBitcode);
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pBadBitcode, &pStream));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_FAILED(pDiaSource->loadDataFromIStream(pStream));
}

static void CompileTestAndLoadDia(dxc::DxcDllSupport &dllSupport, IDiaDataSource **ppDataSource) {
  CComPtr<IDxcBlob> pContainer;
  CComPtr<IDxcBlob> pDebugContent;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;
  CComPtr<IDxcLibrary> pLib;
  CComPtr<IDxcContainerReflection> pReflection;
  UINT32 index;

  VerifyCompileOK(dllSupport, EmptyCompute, L"cs_6_0", L"/Zi", &pContainer);
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  VERIFY_SUCCEEDED(pReflection->Load(pContainer));
  VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &index));
  VERIFY_SUCCEEDED(pReflection->GetPartContent(index, &pDebugContent));
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pDebugContent, &pStream));
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pStream));
  if (ppDataSource) {
    *ppDataSource = pDiaSource.Detach();
  }
}

TEST_F(CompilerTest, DiaLoadDebugThenOK) {
  CompileTestAndLoadDia(m_dllSupport, nullptr);
}

TEST_F(CompilerTest, DiaTableIndexThenOK) {
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IDiaSession> pDiaSession;
  CComPtr<IDiaEnumTables> pEnumTables;
  CComPtr<IDiaTable> pTable;
  VARIANT vtIndex;
  CompileTestAndLoadDia(m_dllSupport, &pDiaSource);
  VERIFY_SUCCEEDED(pDiaSource->openSession(&pDiaSession));
  VERIFY_SUCCEEDED(pDiaSession->getEnumTables(&pEnumTables));

  vtIndex.vt = VT_EMPTY;
  VERIFY_FAILED(pEnumTables->Item(vtIndex, &pTable));

  vtIndex.vt = VT_I4;
  vtIndex.intVal = 1;
  VERIFY_SUCCEEDED(pEnumTables->Item(vtIndex, &pTable));
  VERIFY_IS_NOT_NULL(pTable.p);
  pTable.Release();

  vtIndex.vt = VT_UI4;
  vtIndex.uintVal = 1;
  VERIFY_SUCCEEDED(pEnumTables->Item(vtIndex, &pTable));
  VERIFY_IS_NOT_NULL(pTable.p);
  pTable.Release();

  vtIndex.uintVal = 100;
  VERIFY_FAILED(pEnumTables->Item(vtIndex, &pTable));
}
#endif // _WIN32 - exclude dia stuff

TEST_F(CompilerTest, PixMSAAToSample0) {
  CodeGenTestCheck(L"pix\\msaaLoad.hlsl");
}

TEST_F(CompilerTest, PixRemoveDiscards) {
  CodeGenTestCheck(L"pix\\removeDiscards.hlsl");
}

TEST_F(CompilerTest, PixPixelCounter) {
  CodeGenTestCheck(L"pix\\pixelCounter.hlsl");
}

TEST_F(CompilerTest, PixPixelCounterEarlyZ) {
  CodeGenTestCheck(L"pix\\pixelCounterEarlyZ.hlsl");
}

TEST_F(CompilerTest, PixPixelCounterNoSvPosition) {
  CodeGenTestCheck(L"pix\\pixelCounterNoSvPosition.hlsl");
}

TEST_F(CompilerTest, PixPixelCounterAddPixelCost) {
  CodeGenTestCheck(L"pix\\pixelCounterAddPixelCost.hlsl");
}

TEST_F(CompilerTest, PixConstantColor) {
  CodeGenTestCheck(L"pix\\constantcolor.hlsl");
}

TEST_F(CompilerTest, PixConstantColorInt) {
  CodeGenTestCheck(L"pix\\constantcolorint.hlsl");
}

TEST_F(CompilerTest, PixConstantColorMRT) {
  CodeGenTestCheck(L"pix\\constantcolorMRT.hlsl");
}

TEST_F(CompilerTest, PixConstantColorUAVs) {
  CodeGenTestCheck(L"pix\\constantcolorUAVs.hlsl");
}

TEST_F(CompilerTest, PixConstantColorOtherSIVs) {
  CodeGenTestCheck(L"pix\\constantcolorOtherSIVs.hlsl");
}

TEST_F(CompilerTest, PixConstantColorFromCB) {
  CodeGenTestCheck(L"pix\\constantcolorFromCB.hlsl");
}

TEST_F(CompilerTest, PixConstantColorFromCBint) {
  CodeGenTestCheck(L"pix\\constantcolorFromCBint.hlsl");
}

TEST_F(CompilerTest, PixForceEarlyZ) {
  CodeGenTestCheck(L"pix\\forceEarlyZ.hlsl");
}

TEST_F(CompilerTest, PixDebugBasic) {
  CodeGenTestCheck(L"pix\\DebugBasic.hlsl");
}

TEST_F(CompilerTest, PixDebugUAVSize) {
  CodeGenTestCheck(L"pix\\DebugUAVSize.hlsl");
}

TEST_F(CompilerTest, PixDebugGSParameters) {
  CodeGenTestCheck(L"pix\\DebugGSParameters.hlsl");
}

TEST_F(CompilerTest, PixDebugPSParameters) {
  CodeGenTestCheck(L"pix\\DebugPSParameters.hlsl");
}

TEST_F(CompilerTest, PixDebugVSParameters) {
  CodeGenTestCheck(L"pix\\DebugVSParameters.hlsl");
}

TEST_F(CompilerTest, PixDebugCSParameters) {
  CodeGenTestCheck(L"pix\\DebugCSParameters.hlsl");
}

TEST_F(CompilerTest, PixDebugFlowControl) {
  CodeGenTestCheck(L"pix\\DebugFlowControl.hlsl");
}

TEST_F(CompilerTest, PixDebugPreexistingSVPosition) {
  CodeGenTestCheck(L"pix\\DebugPreexistingSVPosition.hlsl");
}

TEST_F(CompilerTest, PixDebugPreexistingSVVertex) {
  CodeGenTestCheck(L"pix\\DebugPreexistingSVVertex.hlsl");
}

TEST_F(CompilerTest, PixDebugPreexistingSVInstance) {
  CodeGenTestCheck(L"pix\\DebugPreexistingSVInstance.hlsl");
}

TEST_F(CompilerTest, PixAccessTracking) {
  CodeGenTestCheck(L"pix\\AccessTracking.hlsl");
}

TEST_F(CompilerTest, CodeGenAbs1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\abs1.hlsl");
}

TEST_F(CompilerTest, CodeGenAbs2) {
  CodeGenTest(L"..\\CodeGenHLSL\\abs2.hlsl");
}

TEST_F(CompilerTest, CodeGenAllLit) {
  CodeGenTest(L"..\\CodeGenHLSL\\all_lit.hlsl");
}

TEST_F(CompilerTest, CodeGenAllocaAtEntryBlk) {
  CodeGenTest(L"..\\CodeGenHLSL\\alloca_at_entry_blk.hlsl");
}

TEST_F(CompilerTest, CodeGenAddUint64) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\AddUint64.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayArg){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayArg.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayArg2){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayArg2.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayArg3){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayArg3.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayOfStruct){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayOfStruct.hlsl");
}

TEST_F(CompilerTest, CodeGenAsUint) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\asuint.hlsl");
}

TEST_F(CompilerTest, CodeGenAsUint2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\asuint2.hlsl");
}

TEST_F(CompilerTest, CodeGenAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\atomic.hlsl");
}

TEST_F(CompilerTest, CodeGenAtomic2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\atomic2.hlsl");
}

TEST_F(CompilerTest, CodeGenAttributeAtVertex) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\attributeAtVertex.hlsl");
}

TEST_F(CompilerTest, CodeGenAttributeAtVertexNoOpt) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\attributeAtVertexNoOpt.hlsl");
}

TEST_F(CompilerTest, CodeGenBarycentrics) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\barycentrics.hlsl");
}

TEST_F(CompilerTest, CodeGenBarycentrics1) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\barycentrics1.hlsl");
}

TEST_F(CompilerTest, CodeGenBarycentricsThreeSV) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\barycentricsThreeSV.hlsl");
}

TEST_F(CompilerTest, CodeGenBinary1) {
  CodeGenTest(L"..\\CodeGenHLSL\\binary1.hlsl");
}

TEST_F(CompilerTest, CodeGenBitCast) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bitcast.hlsl");
}

TEST_F(CompilerTest, CodeGenBitCast16Bits) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bitcast_16bits.hlsl");
}

TEST_F(CompilerTest, CodeGenBoolComb) {
  CodeGenTest(L"..\\CodeGenHLSL\\boolComb.hlsl");
}

TEST_F(CompilerTest, CodeGenBoolSvTarget) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\boolSvTarget.hlsl");
}

TEST_F(CompilerTest, CodeGenCalcLod2DArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\calcLod2DArray.hlsl");
}

TEST_F(CompilerTest, CodeGenCall1) {
  CodeGenTest(L"..\\CodeGenHLSL\\call1.hlsl");
}

TEST_F(CompilerTest, CodeGenCall3) {
  CodeGenTest(L"..\\CodeGenHLSL\\call3.hlsl");
}

TEST_F(CompilerTest, CodeGenCast1) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast1.hlsl");
}

TEST_F(CompilerTest, CodeGenCast2) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast2.hlsl");
}

TEST_F(CompilerTest, CodeGenCast3) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast3.hlsl");
}

TEST_F(CompilerTest, CodeGenCast4) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast4.hlsl");
}

TEST_F(CompilerTest, CodeGenCast5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cast5.hlsl");
}

TEST_F(CompilerTest, CodeGenCast6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cast6.hlsl");
}

TEST_F(CompilerTest, CodeGenCast7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cast7.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuf_init_static) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuf_init_static.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferCopy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_copy.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferCopy1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_copy1.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferCopy2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_copy2.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferCopy3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_copy3.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferCopy4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_copy4.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferWithFunction) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_fn.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferWithFunctionCopy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer_fn_copy.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer_unused) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer_unused.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer1_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer1.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer1_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer1.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer2_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer2.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer2_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer2.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer3_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer3.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer3_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer3.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer5_51) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer5.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer6_51) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer6.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer64Types) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer64Types.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferAlloc) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferAlloc.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferAllocLegacy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferAlloc_legacy.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferHalf) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferHalf.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferHalfStruct) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferHalf-struct.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferInLoop) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbufferInLoop.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferInt16) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferInt16.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferInt16Struct) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferInt16-struct.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferMinPrec) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferMinPrec.hlsl");
}

TEST_F(CompilerTest, CodeGenClass) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\class.hlsl");
}

TEST_F(CompilerTest, CodeGenClip) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\clip.hlsl");
}

TEST_F(CompilerTest, CodeGenClipPlanes) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\clip_planes.hlsl");
}

TEST_F(CompilerTest, CodeGenConstoperand1) {
  CodeGenTest(L"..\\CodeGenHLSL\\constoperand1.hlsl");
}

TEST_F(CompilerTest, CodeGenConstMat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\constMat.hlsl");
}

TEST_F(CompilerTest, CodeGenConstMat2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\constMat2.hlsl");
}

TEST_F(CompilerTest, CodeGenConstMat3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\constMat3.hlsl");
}

TEST_F(CompilerTest, CodeGenConstMat4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\constMat4.hlsl");
}

TEST_F(CompilerTest, CodeGenCorrectDelay) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\correct_delay.hlsl");
}

TEST_F(CompilerTest, CodeGenDataLayout) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dataLayout.hlsl");
}

TEST_F(CompilerTest, CodeGenDataLayoutHalf) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dataLayoutHalf.hlsl");
}

TEST_F(CompilerTest, CodeGenDiscard) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\discard.hlsl");
}

TEST_F(CompilerTest, CodeGenDivZero) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\divZero.hlsl");
}

TEST_F(CompilerTest, CodeGenDot1) {
  CodeGenTest(L"..\\CodeGenHLSL\\dot1.hlsl");
}

TEST_F(CompilerTest, CodeGenDynamic_Resources) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dynamic-resources.hlsl");
}

TEST_F(CompilerTest, CodeGenEffectSkip) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\effect_skip.hlsl");
}

TEST_F(CompilerTest, CodeGenEliminateDynamicIndexing) {
  CodeGenTestCheck(L"eliminate_dynamic_output.hlsl");
}

TEST_F(CompilerTest, CodeGenEliminateDynamicIndexing2) {
  CodeGenTestCheck(L"eliminate_dynamic_output2.hlsl");
}

TEST_F(CompilerTest, CodeGenEliminateDynamicIndexing3) {
  CodeGenTestCheck(L"eliminate_dynamic_output3.hlsl");
}

TEST_F(CompilerTest, CodeGenEliminateDynamicIndexing4) {
  CodeGenTestCheck(L"eliminate_dynamic_output4.hlsl");
}

TEST_F(CompilerTest, CodeGenEliminateDynamicIndexing6) {
  CodeGenTestCheck(L"eliminate_dynamic_output6.hlsl");
}

TEST_F(CompilerTest, CodeGenEmpty) {
  CodeGenTest(L"..\\CodeGenHLSL\\empty.hlsl");
}

TEST_F(CompilerTest, CodeGenEmptyStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\emptyStruct.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum1) {
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum1.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum2) {
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum2.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum3) {
  if (m_ver.SkipDxilVersion(1,1)) return;
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum3.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum4) {
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum4.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum5) {
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum5.hlsl");
}

TEST_F(CompilerTest, CodeGenEnum6) {
    CodeGenTestCheck(L"..\\CodeGenHLSL\\enum6.hlsl");
}

TEST_F(CompilerTest, CodeGenEarlyDepthStencil) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\earlyDepthStencil.hlsl");
}

TEST_F(CompilerTest, CodeGenEval) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\eval.hlsl");
}

TEST_F(CompilerTest, CodeGenEvalInvalid) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\evalInvalid.hlsl");
}

TEST_F(CompilerTest, CodeGenEvalMat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\evalMat.hlsl");
}

TEST_F(CompilerTest, CodeGenEvalMatMember) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\evalMatMember.hlsl");
}

TEST_F(CompilerTest, CodeGenEvalPos) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\evalPos.hlsl");
}

TEST_F(CompilerTest, CodeGenExternRes) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\extern_res.hlsl");
}

TEST_F(CompilerTest, CodeGenExpandTrig) {
  CodeGenTestCheck(L"expand_trig\\acos.hlsl");
  CodeGenTestCheck(L"expand_trig\\acos_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\asin.hlsl");
  CodeGenTestCheck(L"expand_trig\\asin_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\atan.hlsl");
  CodeGenTestCheck(L"expand_trig\\atan_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\hcos.hlsl");
  CodeGenTestCheck(L"expand_trig\\hcos_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\hsin.hlsl");
  CodeGenTestCheck(L"expand_trig\\hsin_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\htan.hlsl");
  CodeGenTestCheck(L"expand_trig\\htan_h.hlsl");
  CodeGenTestCheck(L"expand_trig\\tan.hlsl");
  CodeGenTestCheck(L"expand_trig\\keep_precise.0.hlsl");
  CodeGenTestCheck(L"expand_trig\\keep_precise.1.hlsl");
}

TEST_F(CompilerTest, CodeGenFloatCast) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\float_cast.hlsl");
}

struct FPEnableExceptionsScope
{
  // _controlfp_s is non-standard and <cfenv> doesn't have a function to enable exceptions
#ifdef _WIN32
  unsigned int previousValue;
  FPEnableExceptionsScope() {
    VERIFY_IS_TRUE(_controlfp_s(&previousValue, 0, _MCW_EM) == 0); // _MCW_EM == 0 means enable all exceptions
  }
  ~FPEnableExceptionsScope() {
    unsigned int newValue;
    errno_t error = _controlfp_s(&newValue, previousValue, _MCW_EM);
    DXASSERT(error == 0, "Failed to restore floating-point environment.");
    (void)error;
  }
#endif
};

TEST_F(CompilerTest, CodeGenFloatingPointEnvironment) {
  FPEnableExceptionsScope fpEnableExceptions;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\fpexcept.hlsl");
}

TEST_F(CompilerTest, CodeGenFloatToBool) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\float_to_bool.hlsl");
}

TEST_F(CompilerTest, CodeGenFirstbitHi) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\firstbitHi.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\firstbitshi_const.hlsl");
}

TEST_F(CompilerTest, CodeGenFirstbitLo) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\firstbitLo.hlsl");
}

TEST_F(CompilerTest, CodeGenFixedWidthTypes) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\fixedWidth.hlsl");
}

TEST_F(CompilerTest, CodeGenFixedWidthTypes16Bit) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\fixedWidth16Bit.hlsl");
}

TEST_F(CompilerTest, CodeGenFloatMaxtessfactor) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\FloatMaxtessfactorHs.hlsl");
}

TEST_F(CompilerTest, CodeGenFModPS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\fmodPs.hlsl");
}

TEST_F(CompilerTest, CodeGenFuncCast) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\func_cast.hlsl");
}

TEST_F(CompilerTest, CodeGenFunctionalCast) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\functionalCast.hlsl");
}

TEST_F(CompilerTest, CodeGenFunctionAttribute) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\functionAttribute.hlsl");
}

TEST_F(CompilerTest, CodeGenGather) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gather.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherCmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherCmp.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherCubeOffset) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherCubeOffset.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherOffset) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherOffset.hlsl");
}

TEST_F(CompilerTest, CodeGenGepZeroIdx) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gep_zero_idx.hlsl");
}

TEST_F(CompilerTest, CodeGenGloballyCoherent) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\globallycoherent.hlsl");
}

TEST_F(CompilerTest, CodeGenI32ColIdx) {
  CodeGenTest(L"..\\CodeGenHLSL\\i32colIdx.hlsl");
}

TEST_F(CompilerTest, CodeGenIcb1) {
  CodeGenTest(L"..\\CodeGenHLSL\\icb1.hlsl");
}

TEST_F(CompilerTest, CodeGenIf1) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if1.hlsl"); }

TEST_F(CompilerTest, CodeGenIf2) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if2.hlsl"); }

TEST_F(CompilerTest, CodeGenIf3) { CodeGenTest(L"..\\CodeGenHLSL\\if3.hlsl"); }

TEST_F(CompilerTest, CodeGenIf4) { CodeGenTest(L"..\\CodeGenHLSL\\if4.hlsl"); }

TEST_F(CompilerTest, CodeGenIf5) { CodeGenTest(L"..\\CodeGenHLSL\\if5.hlsl"); }

TEST_F(CompilerTest, CodeGenIf6) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if6.hlsl"); }

TEST_F(CompilerTest, CodeGenIf7) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if7.hlsl"); }

TEST_F(CompilerTest, CodeGenIf8) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if8.hlsl"); }

TEST_F(CompilerTest, CodeGenIf9) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if9.hlsl"); }

TEST_F(CompilerTest, CodeGenImm0) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\imm0.hlsl");
}

TEST_F(CompilerTest, CodeGenInclude) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Include.hlsl");
}

TEST_F(CompilerTest, CodeGenIncompletePos) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\incompletePos.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput2) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput2.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput3) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput3.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput4) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput4.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableoutput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableoutput1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp2) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp2.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp3) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp3.hlsl");
}

TEST_F(CompilerTest, CodeGenInitListType) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\initlist_type.hlsl");
}

TEST_F(CompilerTest, CodeGenInoutSE) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout_se.hlsl");
}

TEST_F(CompilerTest, CodeGenInout1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout1.hlsl");
}

TEST_F(CompilerTest, CodeGenInout2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout2.hlsl");
}

TEST_F(CompilerTest, CodeGenInout3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout3.hlsl");
}

TEST_F(CompilerTest, CodeGenInout4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout4.hlsl");
}

TEST_F(CompilerTest, CodeGenInout5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout5.hlsl");
}

TEST_F(CompilerTest, CodeGenInput1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\input1.hlsl");
}

TEST_F(CompilerTest, CodeGenInput2) {
  CodeGenTest(L"..\\CodeGenHLSL\\input2.hlsl");
}

TEST_F(CompilerTest, CodeGenInput3) {
  CodeGenTest(L"..\\CodeGenHLSL\\input3.hlsl");
}

TEST_F(CompilerTest, CodeGenInt16Op) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\int16Op.hlsl");
}

TEST_F(CompilerTest, CodeGenInt16OpBits) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\int16OpBits.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic1.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic1Minprec) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic1_minprec.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic2.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic2Minprec) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic2_minprec.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_even) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_even.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_integer) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_integer.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_odd) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_odd.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_pow2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_pow2.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic4.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic4_dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic4_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic5.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic5Minprec) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic5_minprec.hlsl");
}

TEST_F(CompilerTest, CodeGenInvalidInputOutputTypes) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\invalid_input_output_types.hlsl");
}

TEST_F(CompilerTest, CodeGenLegacyStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\legacy_struct.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_cs_entry2.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_cs_entry3.hlsl");
}

TEST_F(CompilerTest, CodeGenLibEntries) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_entries.hlsl");
}

TEST_F(CompilerTest, CodeGenLibEntries2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_entries2.hlsl");
}

TEST_F(CompilerTest, CodeGenLibNoAlias) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_no_alias.hlsl");
}

TEST_F(CompilerTest, CodeGenLibResource) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_resource.hlsl");
}

TEST_F(CompilerTest, CodeGenLibUnusedFunc) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lib_unused_func.hlsl");
}

TEST_F(CompilerTest, CodeGenLitInParen) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lit_in_paren.hlsl");
}

TEST_F(CompilerTest, CodeGenLiteralShift) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\literalShift.hlsl");
}

TEST_F(CompilerTest, CodeGenLiveness1) {
  CodeGenTest(L"..\\CodeGenHLSL\\liveness1.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop1.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource1.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource4.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource7.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes7Dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource7_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop2.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop3.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop4) {
  CodeGenTest(L"..\\CodeGenHLSL\\loop4.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop5) {
  CodeGenTest(L"..\\CodeGenHLSL\\loop5.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop6.hlsl");
}

TEST_F(CompilerTest, CodeGenMatParam) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\mat_param.hlsl");
}

TEST_F(CompilerTest, CodeGenMatParam2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\mat_param2.hlsl");
}

//TEST_F(CompilerTest, CodeGenMatParam3) {
//  CodeGenTestCheck(L"..\\CodeGenHLSL\\mat_param3.hlsl");
//}

TEST_F(CompilerTest, CodeGenMatArrayOutput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\MatArrayOutput.hlsl");
}

TEST_F(CompilerTest, CodeGenMatArrayOutput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\MatArrayOutput2.hlsl");
}

TEST_F(CompilerTest, CodeGenMatElt) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matElt.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInit) {
  CodeGenTest(L"..\\CodeGenHLSL\\matInit.hlsl");
}

TEST_F(CompilerTest, CodeGenMatMulMat) {
  CodeGenTest(L"..\\CodeGenHLSL\\matMulMat.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOps) {
  // TODO: change to CodeGenTestCheck
  CodeGenTest(L"..\\CodeGenHLSL\\matOps.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrix_in_struct.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInStructRet) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrix_in_struct_ret.hlsl");
}

TEST_F(CompilerTest, CodeGenMatIn) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrixIn.hlsl");
}

TEST_F(CompilerTest, CodeGenMatIn1) {
  if (m_ver.SkipIRSensitiveTest()) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrixIn1.hlsl");
}

TEST_F(CompilerTest, CodeGenMatIn2) {
  if (m_ver.SkipIRSensitiveTest()) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrixIn2.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOut) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrixOut.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOut1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrixOut1.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOut2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrixOut2.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript2.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript3.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript4.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript5.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript6.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript7.hlsl");
}

TEST_F(CompilerTest, CodeGenMaxMin) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\max_min.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec1.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec2.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec3.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec4.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec5.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec6.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec7.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprecCoord) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec_coord.hlsl");
}

TEST_F(CompilerTest, CodeGenModf) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\modf.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprecCast) {
  CodeGenTest(L"..\\CodeGenHLSL\\minprec_cast.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprecImm) {
  CodeGenTest(L"..\\CodeGenHLSL\\minprec_imm.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad1.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad2) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad2.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad3.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad4) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad4.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad5) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad5.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad6) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad6.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiUAVLoad7) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiUAVLoad7.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiStream) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiStreamGS.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiStream2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiStreamGS2.hlsl");
}

TEST_F(CompilerTest, CodeGenNeg1) {
  CodeGenTest(L"..\\CodeGenHLSL\\neg1.hlsl");
}

TEST_F(CompilerTest, CodeGenNeg2) {
  CodeGenTest(L"..\\CodeGenHLSL\\neg2.hlsl");
}

TEST_F(CompilerTest, CodeGenNegabs1) {
  CodeGenTest(L"..\\CodeGenHLSL\\negabs1.hlsl");
}

TEST_F(CompilerTest, CodeGenNoise) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\noise.hlsl");
}

TEST_F(CompilerTest, CodeGenNonUniform) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\NonUniform.hlsl");
}

TEST_F(CompilerTest, CodeGenOptForNoOpt) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\optForNoOpt.hlsl");
}

TEST_F(CompilerTest, CodeGenOptForNoOpt2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\optForNoOpt2.hlsl");
}

TEST_F(CompilerTest, CodeGenOptionGis) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\option_gis.hlsl");
}

TEST_F(CompilerTest, CodeGenOptionWX) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\option_WX.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output1.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output2.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output3.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output4.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output5.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output6.hlsl");
}

TEST_F(CompilerTest, CodeGenOutputArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\outputArray.hlsl");
}

TEST_F(CompilerTest, CodeGenPassthrough1) {
  CodeGenTest(L"..\\CodeGenHLSL\\passthrough1.hlsl");
}

TEST_F(CompilerTest, CodeGenPassthrough2) {
  CodeGenTest(L"..\\CodeGenHLSL\\passthrough2.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise1.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise2.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise3.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise4.hlsl");
}

TEST_F(CompilerTest, CodeGenPreciseOnCall) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise_call.hlsl");
}

TEST_F(CompilerTest, CodeGenPreciseOnCallNot) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise_call_not.hlsl");
}

TEST_F(CompilerTest, CodeGenPreserveAllOutputs) {
  CodeGenTestCheck(L"preserve_all_outputs_1.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_2.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_3.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_4.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_5.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_6.hlsl");
  CodeGenTestCheck(L"preserve_all_outputs_7.hlsl");
}

TEST_F(CompilerTest, CodeGenRaceCond2) {
  CodeGenTest(L"..\\CodeGenHLSL\\RaceCond2.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf1) {
  CodeGenTest(L"..\\CodeGenHLSL\\raw_buf1.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf2) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\raw_buf2.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\raw_buf3.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf4) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\raw_buf4.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf5) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\raw_buf5.hlsl");
}

TEST_F(CompilerTest, CodeGenRcp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\rcp1.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput2.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput3.hlsl");
}

TEST_F(CompilerTest, CodeGenRedundantinput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\redundantinput1.hlsl");
}

TEST_F(CompilerTest, CodeGenRes64bit) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\res64bit.hlsl");
}

TEST_F(CompilerTest, CodeGenRovs) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rovs.hlsl");
}

TEST_F(CompilerTest, CodeGenRValAssign) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rval_assign.hlsl");
}

TEST_F(CompilerTest, CodeGenRValSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\RValSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenSample1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sample1.hlsl");
}

TEST_F(CompilerTest, CodeGenSample2) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample2.hlsl");
}

TEST_F(CompilerTest, CodeGenSample3) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample3.hlsl");
}

TEST_F(CompilerTest, CodeGenSample4) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample4.hlsl");
}

TEST_F(CompilerTest, CodeGenSample5) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample5.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleBias) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleBias.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmp.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmpLZ) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmpLZ.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmpLZ2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmpLZ2.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleGrad) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleGrad.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleL) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleL.hlsl");
}

TEST_F(CompilerTest, CodeGenSaturate1) {
  CodeGenTest(L"..\\CodeGenHLSL\\saturate1.hlsl");
}

TEST_F(CompilerTest, CodeGenScalarOnVecIntrinsic) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalarOnVecIntrisic.hlsl");
}

TEST_F(CompilerTest, CodeGenScalarToVec) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalarToVec.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj2.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj3.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj4.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj5.hlsl");
}

TEST_F(CompilerTest, CodeGenSelfCopy) {
  CodeGenTest(L"..\\CodeGenHLSL\\self_copy.hlsl");
}

TEST_F(CompilerTest, CodeGenSelMat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selMat.hlsl");
}

TEST_F(CompilerTest, CodeGenSignaturePacking) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\signature_packing.hlsl");
}

TEST_F(CompilerTest, CodeGenSignaturePackingByWidth) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\signature_packing_by_width.hlsl");
}

TEST_F(CompilerTest, CodeGenShaderAttr) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\shader_attr.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem_Dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem_Phi) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem_phi.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem1.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem2.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem2Dim) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem2Dim.hlsl");
}

TEST_F(CompilerTest, CodeGenShift) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\shift.hlsl");
}

TEST_F(CompilerTest, CodeGenShortCircuiting0) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\short_circuiting0.hlsl");
}

TEST_F(CompilerTest, CodeGenShortCircuiting1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\short_circuiting1.hlsl");
}

TEST_F(CompilerTest, CodeGenShortCircuiting2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\short_circuiting2.hlsl");
}

TEST_F(CompilerTest, CodeGenShortCircuiting3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\short_circuiting3.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleDS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleDs1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS2.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS3.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS4.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS5.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS6.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS7.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS11) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS11.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS12) {
	CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS12.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs2.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs3.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs4.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs5.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs6.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs7.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS8) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs8.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS9) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs9.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS10) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs10.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS11) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHs11.hlsl");
}

TEST_F(CompilerTest, CodeGenSMFail) {
  CodeGenTestCheck(L"sm-fail.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Ms_Load1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\srv_ms_load1.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Ms_Load2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\srv_ms_load2.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Typed_Load1) {
  CodeGenTest(L"..\\CodeGenHLSL\\srv_typed_load1.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Typed_Load2) {
  CodeGenTest(L"..\\CodeGenHLSL\\srv_typed_load2.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticConstGlobal) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\static_const_global.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticConstGlobal2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\static_const_global2.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\staticGlobals.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals2) {
  CodeGenTest(L"..\\CodeGenHLSL\\staticGlobals2.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\staticGlobals3.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals4) {
  CodeGenTest(L"..\\CodeGenHLSL\\staticGlobals4.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\staticGlobals5.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticMatrix) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\static_matrix.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticResource) {
  CodeGenTest(L"..\\CodeGenHLSL\\static_resource.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticResource2) {
  CodeGenTest(L"..\\CodeGenHLSL\\static_resource2.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf1.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf2) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf2.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf3) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf3.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf4) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf4.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf5) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf5.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf6) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf6.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf_New_Layout) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_buf_new_layout.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_BufHasCounter) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_bufHasCounter.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_BufHasCounter2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_bufHasCounter2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructArray) {
  CodeGenTest(L"..\\CodeGenHLSL\\structArray.hlsl");
}

TEST_F(CompilerTest, CodeGenStructCast) {
  CodeGenTest(L"..\\CodeGenHLSL\\StructCast.hlsl");
}

TEST_F(CompilerTest, CodeGenStructCast2) {
  CodeGenTest(L"..\\CodeGenHLSL\\structCast2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer) {
  CodeGenTest(L"..\\CodeGenHLSL\\structInBuffer.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer2) {
  CodeGenTest(L"..\\CodeGenHLSL\\structInBuffer2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\structInBuffer3.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitchFloat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\switch_float.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\switch1.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch2) {
  CodeGenTest(L"..\\CodeGenHLSL\\switch2.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch3) {
  CodeGenTest(L"..\\CodeGenHLSL\\switch3.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzle1) {
  CodeGenTest(L"..\\CodeGenHLSL\\swizzle1.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzle2) {
  CodeGenTest(L"..\\CodeGenHLSL\\swizzle2.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzleAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\swizzleAtomic.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzleAtomic2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\swizzleAtomic2.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzleIndexing) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\swizzleIndexing.hlsl");
}

TEST_F(CompilerTest, CodeGenTemp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\temp1.hlsl");
}

TEST_F(CompilerTest, CodeGenTemp2) {
  CodeGenTest(L"..\\CodeGenHLSL\\temp2.hlsl");
}

TEST_F(CompilerTest, CodeGenTempDbgInfo) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\temp_dbg_info.hlsl");
}

TEST_F(CompilerTest, CodeGenTexSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\TexSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Raw1){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uav_raw1.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Typed_Load_Store1) {
  CodeGenTest(L"..\\CodeGenHLSL\\uav_typed_load_store1.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Typed_Load_Store2) {
  CodeGenTest(L"..\\CodeGenHLSL\\uav_typed_load_store2.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Typed_Load_Store3) {
  if (m_ver.SkipDxilVersion(1,2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uav_typed_load_store3.hlsl");
}

TEST_F(CompilerTest, CodeGenUint64_1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uint64_1.hlsl");
}

TEST_F(CompilerTest, CodeGenUint64_2) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uint64_2.hlsl");
}

TEST_F(CompilerTest, CodeGenUintSample) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uintSample.hlsl");
}

TEST_F(CompilerTest, CodeGenUmaxObjectAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\umaxObjectAtomic.hlsl");
}

TEST_F(CompilerTest, CodeGenUnrollDbg) {
  CodeGenTest(L"..\\CodeGenHLSL\\unroll_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenUnsignedShortHandMatrixVector) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\unsignedShortHandMatrixVector.hlsl");
}

TEST_F(CompilerTest, CodeGenUnusedFunc) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\unused_func.hlsl");
}

TEST_F(CompilerTest, CodeGenUnusedCB) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\unusedCB.hlsl");
}

TEST_F(CompilerTest, CodeGenUpdateCounter) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\updateCounter.hlsl");
}

TEST_F(CompilerTest, CodeGenUpperCaseRegister1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uppercase-register1.hlsl");
}

TEST_F(CompilerTest, CodeGenVcmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vcmp.hlsl");
}

TEST_F(CompilerTest, CodeGenVecBitCast) {
  CodeGenTest(L"..\\CodeGenHLSL\\vec_bitcast.hlsl");
}

TEST_F(CompilerTest, CodeGenVec_Comp_Arg){
  CodeGenTest(L"..\\CodeGenHLSL\\vec_comp_arg.hlsl");
}

TEST_F(CompilerTest, CodeGenVecCmpCond) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vecCmpCond.hlsl");
}

TEST_F(CompilerTest, CodeGenVecTrunc) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vecTrunc.hlsl");
}

TEST_F(CompilerTest, CodeGenWave) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\wave.hlsl");
}

TEST_F(CompilerTest, CodeGenWaveNoOpt) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\wave_no_opt.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteMaskBuf) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeMaskBuf.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteMaskBuf2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeMaskBuf2.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteMaskBuf3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeMaskBuf3.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteMaskBuf4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeMaskBuf4.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput2.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput3.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput4.hlsl");
}

TEST_F(CompilerTest, CodeGenAttributes_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\attributes_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenConst_Exprb_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\const-exprB_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenConst_Expr_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\const-expr_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenFunctions_Mod){
  CodeGenTest(L"..\\CodeGenHLSL\\functions_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenImplicit_Casts_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\implicit-casts_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexing_Operator_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexing-operator_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic_Examples_Mod) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic-examples_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenLiterals_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\literals_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenLiterals_Exact_Precision_Mod) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTest(L"..\\CodeGenHLSL\\literals_exact_precision_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenMatrix_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrix-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenMatrix_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrix-syntax_Mod.hlsl");
}

//TEST_F(CompilerTest, CodeGenMore_Operators_Mod){
//  CodeGenTest(L"..\\CodeGenHLSL\\more-operators_Mod.hlsl");
//}

// TODO: enable this after support local/parameter resource.
//TEST_F(CompilerTest, CodeGenObject_Operators_Mod) {
//  CodeGenTest(L"..\\CodeGenHLSL\\object-operators_Mod.hlsl");
//}

TEST_F(CompilerTest, CodeGenPackreg_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\packreg_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenParentMethod) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\parent_method.hlsl");
}

TEST_F(CompilerTest, CodeGenParameter_Types) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\parameter_types.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Operators_Assign_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-operators-assign_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Operators_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-operators_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenSemantics_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\semantics_Mod.hlsl");
}

// TEST_F(CompilerTest, CodeGenSpec_Mod){
//  CodeGenTest(L"..\\CodeGenHLSL\\spec_Mod.hlsl");
//}

TEST_F(CompilerTest, CodeGenString_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\string_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\struct-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_AssignmentsFull_Mod){
  CodeGenTest(L"..\\CodeGenHLSL\\struct-assignmentsFull_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenTemplate_Checks_Mod) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\template-checks_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenToinclude2_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\toinclude2_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenTypemods_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\typemods-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenTypedBufferHalf) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\typed_buffer_half.hlsl");
}

TEST_F(CompilerTest, CodeGenTypedefNewType) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\typedef_new_type.hlsl");
}

TEST_F(CompilerTest, CodeGenVarmods_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\varmods-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Syntax_Mix_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-syntax-mix_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS2.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS3.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_VS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_VS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_VS2.hlsl");
}

TEST_F(CompilerTest, CodeGenVecIndexingInput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vecIndexingInput.hlsl");
}

TEST_F(CompilerTest, CodeGenVecMulMat) {
  CodeGenTest(L"..\\CodeGenHLSL\\vecMulMat.hlsl");
}

TEST_F(CompilerTest, CodeGenVecArrayParam) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vector_array_param.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings1.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings2.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings2.hlsl");
}

TEST_F(CompilerTest, CodeGenResCopy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resCopy.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-struct.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceParam) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource_param.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCBV) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cbv.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTB) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tb.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTBV) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tbv.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInStruct2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-struct2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInStruct3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-struct3.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb3.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb4.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCBV2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cbv2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTB2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tb2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTBV2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tbv2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceArrayParam) {
  CodeGenTest(L"..\\CodeGenHLSL\\resource-array-param.hlsl");
}

TEST_F(CompilerTest, CodeGenResPhi) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resPhi.hlsl");
}

TEST_F(CompilerTest, CodeGenResPhi2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resPhi2.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigEntry) {
  CodeGenTest(L"..\\CodeGenHLSL\\rootSigEntry.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile) {
  CodeGenTest(L"..\\CodeGenHLSL\\rootSigProfile.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile2) {
  // TODO: Verify the result when reflect the structures.
  CodeGenTest(L"..\\CodeGenHLSL\\rootSigProfile2.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigProfile3.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigProfile4.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile5) {
  CodeGenTest(L"..\\CodeGenHLSL\\rootSigProfile5.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine1.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine2.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine3.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine4.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine5.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine6.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine7.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine8) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine8.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine9) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine9.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine10) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine10.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigDefine11) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rootSigDefine11.hlsl");
}

TEST_F(CompilerTest, CodeGenCBufferStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer-struct.hlsl");
}

TEST_F(CompilerTest, CodeGenCBufferStructArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer-structarray.hlsl");
}

TEST_F(CompilerTest, CodeGenPatchLength) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\PatchLength1.hlsl");
}

// Dx11 Sample

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Blurx_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_BlurX_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Blury_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_BlurY_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hdecode){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HDecode.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Encodeblockcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_EncodeBlockCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Trymodeg10Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_TryModeG10CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Trymodele10Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_TryModeLE10CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Decode){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Decode.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Encodeblockcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_EncodeBlockCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode02Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode02CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode137Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode137CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode456Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode456CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Brightpassandhorizfiltercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BrightPassAndHorizFilterCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Computeshadersort11){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ComputeShaderSort11.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Computeshadersort11_Matrixtranspose){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ComputeShaderSort11_MatrixTranspose.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Sm_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_SM_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Dumptotexture){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DumpToTexture.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Filtercs_Horz){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FilterCS_Horz.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Filtercs_Vertical){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FilterCS_Vertical.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Finalpass_Cpu_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FinalPass_CPU_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Finalpass_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FinalPass_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Buildgridcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_BuildGridCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Buildgridindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_BuildGridIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Cleargridindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ClearGridIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Grid){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Grid.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Shared){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Shared.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Simple){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Simple.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Grid){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Grid.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Shared){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Shared.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Simple){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Simple.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Integratecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_IntegrateCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Rearrangeparticlescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_RearrangeParticlesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidrender_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidRender_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidrender_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidRender_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Nbodygravitycs11){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\NBodyGravityCS11.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Createprefixsum_Pass0_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_CreatePrefixSum_Pass0_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Createprefixsum_Pass1_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_CreatePrefixSum_Pass1_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Fragmentcountps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_FragmentCountPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Sortandrendercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_SortAndRenderCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particledraw_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ParticleDraw_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particledraw_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ParticleDraw_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pom_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\POM_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pom_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\POM_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Bloomps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_BloomPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale2X2_Lumps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale2x2_LumPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale3X3Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale3x3PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale3X3_Brightpassps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale3x3_BrightPassPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Finalpassps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_FinalPassPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Reduceto1Dcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ReduceTo1DCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Reducetosinglecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ReduceToSingleCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Rendervariancesceneps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\RenderVarianceScenePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Rendervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\RenderVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Bezierevalds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_BezierEvalDS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Meshskinningvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_MeshSkinningVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Patchskinningvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_PatchSkinningVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Smoothps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SmoothPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Subdtobezierhs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SubDToBezierHS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Subdtobezierhs4444){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SubDToBezierHS4444.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Edgefactorcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_EdgeFactorCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Numverticesindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_NumVerticesIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Scatteridcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_ScatterIDCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Tessellateindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_TessellateIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Tessellateverticescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_TessellateVerticesCS.hlsl");
}

// Dx12 Sample

TEST_F(CompilerTest, CodeGenSamplesD12_DynamicIndexing_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_dynamic_indexing_pixel.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_ExecuteIndirect_CS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_execute_indirect_cs.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_MultiThreading_VS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_multithreading_vs.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_MultiThreading_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_multithreading_ps.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_NBodyGravity_CS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_nBodyGravityCS.hlsl");
}

// Dx12 sample/MiniEngine
TEST_F(CompilerTest, CodeGenDx12MiniEngineAdaptexposurecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AdaptExposureCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsampleblendoutcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsampleBlendOutCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsampleCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplepreminblendoutcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsamplePreMinBlendOutCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplepremincs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsamplePreMinCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAopreparedepthbuffers1Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoPrepareDepthBuffers1CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAopreparedepthbuffers2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoPrepareDepthBuffers2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAorender1Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoRender1CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAorender2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoRender2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineApplybloomcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ApplyBloomCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAveragelumacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AverageLumaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubichorizontalupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicHorizontalUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicupsamplegammaps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicUpsampleGammaPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicverticalupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicVerticalUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBilinearupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BilinearUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBloomextractanddownsamplehdrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BloomExtractAndDownsampleHdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBloomextractanddownsampleldrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BloomExtractAndDownsampleLdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBlurcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BlurCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBuffercopyps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BufferCopyPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameramotionblurprepasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraMotionBlurPrePassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameramotionblurprepasslinearzcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraMotionBlurPrePassLinearZCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameravelocitycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraVelocityCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineConvertldrtodisplayaltps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ConvertLDRToDisplayAltPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineConvertldrtodisplayps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ConvertLDRToDisplayPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugdrawhistogramcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugDrawHistogramCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugluminancehdrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugLuminanceHdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugluminanceldrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugLuminanceLdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugssaocs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugSSAOCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDepthviewerps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DepthViewerPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDepthviewervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DepthViewerVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDownsamplebloomallcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DownsampleBloomAllCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDownsamplebloomcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DownsampleBloomCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineExtractlumacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ExtractLumaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass1_Luma_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass1_Luma_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass1_Rgb_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass1_RGB_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Hcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2HCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Hdebugcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2HDebugCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Vcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2VCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Vdebugcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2VDebugCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaaresolveworkqueuecs){
  if (m_ver.SkipIRSensitiveTest()) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAResolveWorkQueueCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratehistogramcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateHistogramCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddxcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddXCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddYCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddxcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddXCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddYCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineLinearizedepthcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\LinearizeDepthCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMagnifypixelsps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MagnifyPixelsPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineModelviewerps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ModelViewerPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineModelviewervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ModelViewerVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurfinalpasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurFinalPassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurfinalpasstemporalcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurFinalPassTemporalCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurprepasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurPrePassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlebincullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleBinCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticledepthboundscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleDepthBoundsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticledispatchindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleDispatchIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlefinaldispatchindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleFinalDispatchIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleinnersortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleInnerSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlelargebincullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleLargeBinCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleoutersortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleOuterSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlepresortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticlePreSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticlePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlesortindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleSortIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlespawncs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleSpawnCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilecullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilecullingcs_fail_unroll){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileCullingCS_fail_unroll.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerendercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastdynamiccs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastDynamicCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastlowrescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastLowResCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderslowdynamiccs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderSlowDynamicCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderslowlowrescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderSlowLowResCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleupdatecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleUpdateCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlevs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphbackgroundvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphBackgroundVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineScreenquadvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ScreenQuadVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineSharpeningupsamplegammaps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\SharpeningUpsampleGammaPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineSharpeningupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\SharpeningUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTemporalblendcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TemporalBlendCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextantialiasps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextAntialiasPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextshadowps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextShadowPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTonemap2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ToneMap2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTonemapcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ToneMapCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineUpsampleandblurcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\UpsampleAndBlurCS.hlsl");
}

TEST_F(CompilerTest, DxilGen_StoreOutput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dxilgen_storeoutput.hlsl");
}

TEST_F(CompilerTest, ConstantFolding) {
  CodeGenTestCheck(L"constprop\\FAbs.hlsl");
  CodeGenTestCheck(L"constprop\\Saturate_half.hlsl");
  CodeGenTestCheck(L"constprop\\Saturate_float.hlsl");
  CodeGenTestCheck(L"constprop\\Saturate_double.hlsl");
  CodeGenTestCheck(L"constprop\\Cos.hlsl");
  CodeGenTestCheck(L"constprop\\Sin.hlsl");
  CodeGenTestCheck(L"constprop\\Tan.hlsl");
  CodeGenTestCheck(L"constprop\\Acos.hlsl");
  CodeGenTestCheck(L"constprop\\Asin.hlsl");
  CodeGenTestCheck(L"constprop\\Atan.hlsl");
  CodeGenTestCheck(L"constprop\\Hcos.hlsl");
  CodeGenTestCheck(L"constprop\\Hsin.hlsl");
  CodeGenTestCheck(L"constprop\\Htan.hlsl");
  CodeGenTestCheck(L"constprop\\Exp.hlsl");
  CodeGenTestCheck(L"constprop\\Frc.hlsl");
  CodeGenTestCheck(L"constprop\\Log.hlsl");
  CodeGenTestCheck(L"constprop\\Sqrt.hlsl");
  CodeGenTestCheck(L"constprop\\Rsqrt.hlsl");
  CodeGenTestCheck(L"constprop\\Round_ne.hlsl");
  CodeGenTestCheck(L"constprop\\Round_ni.hlsl");
  CodeGenTestCheck(L"constprop\\Round_pi.hlsl");
  CodeGenTestCheck(L"constprop\\Round_z.hlsl");
  
  CodeGenTestCheck(L"constprop\\Bfrev.hlsl");
  CodeGenTestCheck(L"constprop\\Countbits.hlsl");
  CodeGenTestCheck(L"constprop\\Firstbitlo.hlsl");
  CodeGenTestCheck(L"constprop\\Firstbithi.hlsl");

  CodeGenTestCheck(L"constprop\\FMin.hlsl");
  CodeGenTestCheck(L"constprop\\FMax.hlsl");
  CodeGenTestCheck(L"constprop\\IMin.hlsl");
  CodeGenTestCheck(L"constprop\\IMax.hlsl");
  CodeGenTestCheck(L"constprop\\UMin.hlsl");
  CodeGenTestCheck(L"constprop\\UMax.hlsl");
  
  CodeGenTestCheck(L"constprop\\FMad.hlsl");
  CodeGenTestCheck(L"constprop\\Fma.hlsl");
  CodeGenTestCheck(L"constprop\\IMad.hlsl");
  CodeGenTestCheck(L"constprop\\UMad.hlsl");
  
  CodeGenTestCheck(L"constprop\\Dot2.hlsl");
  CodeGenTestCheck(L"constprop\\Dot3.hlsl");
  CodeGenTestCheck(L"constprop\\Dot4.hlsl");

  CodeGenTestCheck(L"constprop\\ibfe.ll");
  CodeGenTestCheck(L"constprop\\ubfe.ll");
  CodeGenTestCheck(L"constprop\\bfi.ll");
}

TEST_F(CompilerTest, HoistConstantArray) {
  CodeGenTestCheck(L"hca\\01.hlsl");
  CodeGenTestCheck(L"hca\\02.hlsl");
  CodeGenTestCheck(L"hca\\03.hlsl");
  CodeGenTestCheck(L"hca\\04.hlsl");
  CodeGenTestCheck(L"hca\\05.hlsl");
  CodeGenTestCheck(L"hca\\06.hlsl");
  CodeGenTestCheck(L"hca\\07.hlsl");
  CodeGenTestCheck(L"hca\\08.hlsl");
  CodeGenTestCheck(L"hca\\09.hlsl");
  CodeGenTestCheck(L"hca\\10.hlsl");
  CodeGenTestCheck(L"hca\\11.hlsl");
  CodeGenTestCheck(L"hca\\12.hlsl");
  CodeGenTestCheck(L"hca\\13.hlsl");
  CodeGenTestCheck(L"hca\\14.hlsl");
  CodeGenTestCheck(L"hca\\15.ll");
}

TEST_F(CompilerTest, VecElemConstEval) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vec_elem_const_eval.hlsl");
}

TEST_F(CompilerTest, PreprocessWhenValidThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[2];
  defines[0].Name = L"MYDEF";
  defines[0].Value = L"int";
  defines[1].Name = L"MYOTHERDEF";
  defines[1].Value = L"123";
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "// First line\r\n"
    "MYDEF g_int = MYOTHERDEF;\r\n"
    "#define FOO BAR\r\n"
    "int FOO;", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Preprocess(pSource, L"file.hlsl", nullptr, 0,
                                         defines, _countof(defines), nullptr,
                                         &pResult));
  HRESULT hrOp;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrOp));
  VERIFY_SUCCEEDED(hrOp);

  CComPtr<IDxcBlob> pOutText;
  VERIFY_SUCCEEDED(pResult->GetResult(&pOutText));
  std::string text(BlobToUtf8(pOutText));
  VERIFY_ARE_EQUAL_STR(
    "#line 1 \"file.hlsl\"\n"
    "\n"
    "int g_int = 123;\n"
    "\n"
    "int BAR;\n", text.c_str());
}

TEST_F(CompilerTest, PreprocessWhenExpandTokenPastingOperandThenAccept) {
  // Tests that we can turn on fxc's behavior (pre-expanding operands before
  // performing token-pasting) using -flegacy-macro-expansion

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  LPCWSTR expandOption = L"-flegacy-macro-expansion";

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

  CreateBlobFromText(R"(
#define SET_INDEX0                10
#define BINDING_INDEX0            5

#define SET(INDEX)                SET_INDEX##INDEX
#define BINDING(INDEX)            BINDING_INDEX##INDEX

#define SET_BIND(NAME,SET,BIND)   resource_set_##SET##_bind_##BIND##_##NAME

#define RESOURCE(NAME,INDEX)      SET_BIND(NAME, SET(INDEX), BINDING(INDEX))

    Texture2D<float4> resource_set_10_bind_5_tex;

  float4 main() : SV_Target{
    return RESOURCE(tex, 0)[uint2(1, 2)];
  }
)",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Preprocess(pSource, L"file.hlsl", &expandOption,
                                         1, nullptr, 0, nullptr, &pResult));
  HRESULT hrOp;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrOp));
  VERIFY_SUCCEEDED(hrOp);

  CComPtr<IDxcBlob> pOutText;
  VERIFY_SUCCEEDED(pResult->GetResult(&pOutText));
  std::string text(BlobToUtf8(pOutText));
  VERIFY_ARE_EQUAL_STR(R"(#line 1 "file.hlsl"
#line 12 "file.hlsl"
    Texture2D<float4> resource_set_10_bind_5_tex;

  float4 main() : SV_Target{
    return resource_set_10_bind_5_tex[uint2(1, 2)];
  }
)",
                       text.c_str());
}

TEST_F(CompilerTest, WhenSigMismatchPCFunctionThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "struct PSSceneIn \n\
    { \n\
      float4 pos  : SV_Position; \n\
      float2 tex  : TEXCOORD0; \n\
      float3 norm : NORMAL; \n\
    }; \n"
    "struct HSPerPatchData {  \n\
      float edges[ 3 ] : SV_TessFactor; \n\
      float inside : SV_InsideTessFactor; \n\
      float foo : FOO; \n\
    }; \n"
    "HSPerPatchData HSPerPatchFunc( InputPatch< PSSceneIn, 3 > points, \n\
      OutputPatch<PSSceneIn, 3> outpoints) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = points[0].tex.x + outpoints[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
    "[domain(\"tri\")] \n\
    [partitioning(\"fractional_odd\")] \n\
    [outputtopology(\"triangle_cw\")] \n\
    [patchconstantfunc(\"HSPerPatchFunc\")] \n\
    [outputcontrolpoints(3)] \n"
    "void main(const uint id : SV_OutputControlPointID, \n\
               const InputPatch< PSSceneIn, 3 > points ) { \n\
    } \n"
    , &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"hs_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find(
    "Signature element SV_Position, referred to by patch constant function, is not found in corresponding hull shader output."));
}

TEST_F(CompilerTest, ViewID) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid01.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid02.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid03.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid04.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid05.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid06.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid07.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid08.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid09.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid10.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid11.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid12.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid13.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid14.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid15.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid16.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid17.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid18.hlsl");
  CodeGenTestCheck(L"..\\CodeGenHLSL\\viewid\\viewid19.hlsl");
}

TEST_F(CompilerTest, SubobjectCodeGenErrors) {
  struct SubobjectErrorTestCase {
    const char *shaderText;
    const char *expectedError;
  };
  SubobjectErrorTestCase testCases[] = {
    { "GlobalRootSignature grs;",           "1:1: error: subobject needs to be initialized" },
    { "StateObjectConfig soc;",             "1:1: error: subobject needs to be initialized" },
    { "LocalRootSignature lrs;",            "1:1: error: subobject needs to be initialized" },
    { "SubobjectToExportsAssociation sea;", "1:1: error: subobject needs to be initialized" },
    { "RaytracingShaderConfig rsc;",        "1:1: error: subobject needs to be initialized" },
    { "RaytracingPipelineConfig rpc;",      "1:1: error: subobject needs to be initialized" },
    { "TriangleHitGroup hitGt;",            "1:1: error: subobject needs to be initialized" },
    { "ProceduralPrimitiveHitGroup hitGt;", "1:1: error: subobject needs to be initialized" },
    { "GlobalRootSignature grs2 = {\"\"};", "1:29: error: empty string not expected here" },
    { "LocalRootSignature lrs2 = {\"\"};",  "1:28: error: empty string not expected here" },
    { "SubobjectToExportsAssociation sea2 = { \"\", \"x\" };", "1:40: error: empty string not expected here" },
    { "string s; SubobjectToExportsAssociation sea4 = { \"x\", s };", "1:55: error: cannot convert to constant string" },
    { "extern int v; RaytracingPipelineConfig rpc2 = { v + 16 };", "1:49: error: cannot convert to constant unsigned int" },
    { "string s; TriangleHitGroup trHitGt2_8 = { s, \"foo\" };", "1:43: error: cannot convert to constant string" },
    { "string s; ProceduralPrimitiveHitGroup ppHitGt2_8 = { s, \"\", s };", "1:54: error: cannot convert to constant string" },
    { "ProceduralPrimitiveHitGroup ppHitGt2_9 = { \"a\", \"b\", \"\"};", "1:54: error: empty string not expected here" }
  };

  for (unsigned i = 0; i < _countof(testCases); i++) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

    CreateBlobFromText(testCases[i].shaderText, &pSource);
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"", L"lib_6_4", nullptr, 0, nullptr, 0, nullptr, &pResult));
    std::string failLog(VerifyOperationFailed(pResult));
    VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find(testCases[i].expectedError));
  }
}

TEST_F(CompilerTest, Unroll) {
  using namespace WEX::TestExecution;
  std::wstring suitePath = L"..\\CodeGenHLSL\\unroll";

  WEX::Common::String value;
  if (!DXC_FAILED(RuntimeParameters::TryGetValue(L"SuitePath", value)))
  {
    suitePath = value;
  }

  CodeGenTestCheckBatchDir(suitePath);
}

TEST_F(CompilerTest, ShaderCompatSuite) {
  using namespace WEX::TestExecution;
  std::wstring suitePath = L"..\\CodeGenHLSL\\shader-compat-suite";

  WEX::Common::String value;
  if (!DXC_FAILED(RuntimeParameters::TryGetValue(L"SuitePath", value)))
  {
    suitePath = value;
  }

  CodeGenTestCheckBatchDir(suitePath);
}

TEST_F(CompilerTest, QuickTest) {
  CodeGenTestCheckBatchDir(L"..\\CodeGenHLSL\\quick-test");
}

TEST_F(CompilerTest, QuickLlTest) {
	CodeGenTestCheckBatchDir(L"..\\CodeGenHLSL\\quick-ll-test");
}

#ifdef _WIN32
TEST_F(CompilerTest, SingleFileCheckTest) {
#else
TEST_F(CompilerTest, DISABLED_SingleFileCheckTest) {
#endif
  using namespace llvm;
  using namespace WEX::TestExecution;
  WEX::Common::String value;
  VERIFY_SUCCEEDED(RuntimeParameters::TryGetValue(L"InputFile", value));
  std::wstring filename = value;
  CW2A pUtf8Filename(filename.c_str());
  if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
    filename = hlsl_test::GetPathToHlslDataFile(filename.c_str());
  }

  CodeGenTestCheckBatch(filename.c_str(), 0);
}
