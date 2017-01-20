///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// ExtensionTest.cpp                                                         //
//                                                                           //
// Provides tests for the language extension APIs.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "CompilationResult.h"
#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/HLSL/HLOperationLowerExtension.h"
#include "dxc/HlslIntrinsicOp.h"

///////////////////////////////////////////////////////////////////////////////
// Support for test intrinsics.

// $result = test_fn(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnArgs[] = {
  { "test_fn", AR_QUAL_OUT, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C }
};

// void test_proc(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestProcArgs[] = {
  { "test_proc", 0, 0, LITEMPLATE_VOID, 0, LICOMPTYPE_VOID, 0, 0 },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C }
};

// $result = test_poly(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnCustomArgs[] = {
  { "test_poly", AR_QUAL_OUT, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C }
};

// $result = test_int(int<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnIntArgs[] = {
  { "test_int", AR_QUAL_OUT, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_INT, 1, IA_C },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_INT, 1, IA_C }
};

// $result = test_nolower(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnNoLowerArgs[] = {
  { "test_nolower", AR_QUAL_OUT, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C }
};

// void test_pack_0(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnPack0[] = {
  { "test_pack_0", 0, 0, LITEMPLATE_VOID, 0, LICOMPTYPE_VOID, 0, 0 },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C }
};

// $result = test_pack_1()
static const HLSL_INTRINSIC_ARGUMENT TestFnPack1[] = {
  { "test_pack_1", AR_QUAL_OUT, 0, LITEMPLATE_VECTOR, 0, LICOMPTYPE_FLOAT, 1, 2 },
};

// $result = test_pack_2(any_vector<any_cardinality> value1, any_vector<any_cardinality> value2)
static const HLSL_INTRINSIC_ARGUMENT TestFnPack2[] = {
  { "test_pack_2", AR_QUAL_OUT, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
  { "value1", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
  { "value2", AR_QUAL_IN, 1, LITEMPLATE_ANY, 1, LICOMPTYPE_NUMERIC, 1, IA_C },
};

// $scalar = test_pack_3(any_vector<any_cardinality> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnPack3[] = {
  { "test_pack_3", AR_QUAL_OUT, 0, LITEMPLATE_SCALAR, 0, LICOMPTYPE_FLOAT, 1, 1 },
  { "value1", AR_QUAL_IN, 1, LITEMPLATE_VECTOR, 1, LICOMPTYPE_FLOAT, 1, 2},
};

// float<2> = test_pack_4(float<3> value)
static const HLSL_INTRINSIC_ARGUMENT TestFnPack4[] = {
  { "test_pack_4", AR_QUAL_OUT, 0, LITEMPLATE_SCALAR, 0, LICOMPTYPE_FLOAT, 1, 2 },
  { "value", AR_QUAL_IN, 1, LITEMPLATE_VECTOR, 1, LICOMPTYPE_FLOAT, 1, 3},
};

// float<2> = test_rand()
static const HLSL_INTRINSIC_ARGUMENT TestRand[] = {
  { "test_rand", AR_QUAL_OUT, 0, LITEMPLATE_SCALAR, 0, LICOMPTYPE_FLOAT, 1, 2 },
};

// uint = test_rand(uint x)
static const HLSL_INTRINSIC_ARGUMENT TestUnsigned[] = {
  { "test_unsigned", AR_QUAL_OUT, 0, LITEMPLATE_SCALAR, 0, LICOMPTYPE_UINT, 1, 1 },
  { "x", AR_QUAL_IN, 1, LITEMPLATE_VECTOR, 1, LICOMPTYPE_UINT, 1, 1},
};

struct Intrinsic {
  LPCWSTR hlslName;
  const char *dxilName;
  const char *strategy;
  HLSL_INTRINSIC hlsl;
};
const char * DEFAULT_NAME = "";

// llvm::array_lengthof that returns a UINT instead of size_t
template <class T, std::size_t N>
UINT countof(T(&)[N]) { return static_cast<UINT>(N); }

Intrinsic Intrinsics[] = {
  {L"test_fn",      DEFAULT_NAME,      "r", {  1, false, true, -1, countof(TestFnArgs), TestFnArgs }},
  {L"test_proc",    DEFAULT_NAME,      "r", {  2, false, true, -1, countof(TestProcArgs), TestProcArgs }},
  {L"test_poly",    "test_poly.$o",    "r", {  3, false, true, -1, countof(TestFnCustomArgs), TestFnCustomArgs }},
  {L"test_int",     "test_int",        "r", {  4, false, true, -1, countof(TestFnIntArgs), TestFnIntArgs}},
  {L"test_nolower", "test_nolower.$o", "n", {  5, false, true, -1, countof(TestFnNoLowerArgs), TestFnNoLowerArgs}},
  {L"test_pack_0",  "test_pack_0.$o",  "p", {  6, false, true, -1, countof(TestFnPack0), TestFnPack0}},
  {L"test_pack_1",  "test_pack_1.$o",  "p", {  7, false, true, -1, countof(TestFnPack1), TestFnPack1}},
  {L"test_pack_2",  "test_pack_2.$o",  "p", {  8, false, true, -1, countof(TestFnPack2), TestFnPack2}},
  {L"test_pack_3",  "test_pack_3.$o",  "p", {  9, false, true, -1, countof(TestFnPack3), TestFnPack3}},
  {L"test_pack_4",  "test_pack_4.$o",  "p", { 10, false, true, -1, countof(TestFnPack4), TestFnPack4}},
  {L"test_rand",    "test_rand",       "r", { 11, false, false,-1, countof(TestRand), TestRand}},
  // Make this intrinsic have the same opcode as an hlsl intrinsic with an unsigned
  // counterpart for testing purposes.
  {L"test_unsigned","test_unsigned",   "n", { static_cast<unsigned>(hlsl::IntrinsicOp::IOP_min), false, true, -1, countof(TestUnsigned), TestUnsigned}},
};

class TestIntrinsicTable : public IDxcIntrinsicTable {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef);
public:
  TestIntrinsicTable() : m_dwRef(0) { }
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  __override HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) {
    return DoBasicQueryInterface<IDxcIntrinsicTable>(this, iid, ppvObject);
  }

  __override HRESULT STDMETHODCALLTYPE
  GetTableName(_Outptr_ LPCSTR *pTableName) {
    *pTableName = "test";
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE LookupIntrinsic(
      LPCWSTR typeName, LPCWSTR functionName, const HLSL_INTRINSIC **pIntrinsic,
      _Inout_ UINT64 *pLookupCookie) {
    if (typeName != nullptr && *typeName) return E_FAIL;
    Intrinsic *intrinsic =
      std::find_if(std::begin(Intrinsics), std::end(Intrinsics), [functionName](const Intrinsic &i) {
        return wcscmp(i.hlslName, functionName) == 0;
    });
    if (intrinsic == std::end(Intrinsics))
      return E_FAIL;

    *pIntrinsic = &intrinsic->hlsl;
    *pLookupCookie = 0;
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE
  GetLoweringStrategy(UINT opcode, _Outptr_ LPCSTR *pStrategy) {
    Intrinsic *intrinsic =
      std::find_if(std::begin(Intrinsics), std::end(Intrinsics), [opcode](const Intrinsic &i) {
      return i.hlsl.Op == opcode;
    });
    
    if (intrinsic == std::end(Intrinsics))
      return E_FAIL;

    *pStrategy = intrinsic->strategy;

    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE
  GetIntrinsicName(UINT opcode, _Outptr_ LPCSTR *pName) {
    Intrinsic *intrinsic =
      std::find_if(std::begin(Intrinsics), std::end(Intrinsics), [opcode](const Intrinsic &i) {
      return i.hlsl.Op == opcode;
    });

    if (intrinsic == std::end(Intrinsics))
      return E_FAIL;

    *pName = intrinsic->dxilName;
    return S_OK;
  }
};

// A class to test semantic define validation.
// It takes a list of defines that when present should cause errors
// and defines that should cause warnings. A more realistic validator
// would look at the values and make sure (for example) they are
// the correct type (integer, string, etc).
class TestSemanticDefineValidator : public IDxcSemanticDefineValidator {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef);
  std::vector<std::string> m_errorDefines;
  std::vector<std::string> m_warningDefines;
public:
  TestSemanticDefineValidator(const std::vector<std::string> &errorDefines, const std::vector<std::string> &warningDefines)
    : m_dwRef(0)
    , m_errorDefines(errorDefines)
    , m_warningDefines(warningDefines)
  { }
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

    __override HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) {
    return DoBasicQueryInterface<IDxcSemanticDefineValidator>(this, iid, ppvObject);
  }

  virtual HRESULT STDMETHODCALLTYPE GetSemanticDefineWarningsAndErrors(LPCSTR pName, LPCSTR pValue, IDxcBlobEncoding **ppWarningBlob, IDxcBlobEncoding **ppErrorBlob) {
    if (!pName || !pValue || !ppWarningBlob || !ppErrorBlob)
      return E_FAIL;

    auto Check = [pName](const std::vector<std::string> &errors, IDxcBlobEncoding **blob) {
      if (std::find(errors.begin(), errors.end(), pName) != errors.end()) {
        dxc::DxcDllSupport dllSupport;
        VERIFY_SUCCEEDED(dllSupport.Initialize());
        std::string error("bad define: ");
        error.append(pName);
        Utf8ToBlob(dllSupport, error.c_str(), blob);
      }
    };
    Check(m_errorDefines, ppErrorBlob);
    Check(m_warningDefines, ppWarningBlob);

    return S_OK;
  }
};
static void CheckOperationFailed(IDxcOperationResult *pResult) {
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_FAILED(status);
}

static std::string GetCompileErrors(IDxcOperationResult *pResult) {
  CComPtr<IDxcBlobEncoding> pErrors;
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
  if (!pErrors)
    return "";
  return BlobToUtf8(pErrors);
}

class Compiler {
public:
  Compiler(dxc::DxcDllSupport &dll) : m_dllSupport(dll) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler.QueryInterface(&pLangExtensions));
  }
  void RegisterSemanticDefine(LPCWSTR define) {
    VERIFY_SUCCEEDED(pLangExtensions->RegisterSemanticDefine(define));
  }
  void RegisterSemanticDefineExclusion(LPCWSTR define) {
    VERIFY_SUCCEEDED(pLangExtensions->RegisterSemanticDefineExclusion(define));
  }
  void SetSemanticDefineValidator(IDxcSemanticDefineValidator *validator) {
    pTestSemanticDefineValidator = validator;
    VERIFY_SUCCEEDED(pLangExtensions->SetSemanticDefineValidator(pTestSemanticDefineValidator));
  }
  void SetSemanticDefineMetaDataName(const char *name) {
    VERIFY_SUCCEEDED(pLangExtensions->SetSemanticDefineMetaDataName("test.defs"));
  }
  void RegisterIntrinsicTable(IDxcIntrinsicTable *table) {
    pTestIntrinsicTable = table;
    VERIFY_SUCCEEDED(pLangExtensions->RegisterIntrinsicTable(pTestIntrinsicTable));
  }
  
  IDxcOperationResult *Compile(const char *program) {
    return Compile(program, {}, {});
  }

  IDxcOperationResult *Compile(const char *program, const std::vector<LPCWSTR> &arguments, const std::vector<DxcDefine> defs ) {
    Utf8ToBlob(m_dllSupport, program, &pCodeBlob);
    VERIFY_SUCCEEDED(pCompiler->Compile(pCodeBlob, L"hlsl.hlsl", L"main",
      L"ps_6_0",
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

  dxc::DxcDllSupport &m_dllSupport;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcLangExtensions> pLangExtensions;
  CComPtr<IDxcBlobEncoding> pCodeBlob;
  CComPtr<IDxcOperationResult> pCompileResult;
  CComPtr<IDxcSemanticDefineValidator> pTestSemanticDefineValidator;
  CComPtr<IDxcIntrinsicTable> pTestIntrinsicTable;
};

///////////////////////////////////////////////////////////////////////////////
// Extension unit tests.

class ExtensionTest {
public:
  BEGIN_TEST_CLASS(ExtensionTest)
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  dxc::DxcDllSupport m_dllSupport;

  TEST_METHOD(DefineWhenRegisteredThenPreserved);
  TEST_METHOD(DefineValidationError);
  TEST_METHOD(DefineValidationWarning);
  TEST_METHOD(DefineNoValidatorOk);
  TEST_METHOD(IntrinsicWhenAvailableThenUsed);
  TEST_METHOD(CustomIntrinsicName);
  TEST_METHOD(NoLowering);
  TEST_METHOD(PackedLowering);
  TEST_METHOD(ReplicateLoweringWhenOnlyVectorIsResult);
  TEST_METHOD(UnsignedOpcodeIsUnchanged);
};

TEST_F(ExtensionTest, DefineWhenRegisteredThenPreserved) {
  Compiler c(m_dllSupport);
  c.RegisterSemanticDefine(L"FOO*");
  c.RegisterSemanticDefineExclusion(L"FOOBAR");
  c.SetSemanticDefineValidator(new TestSemanticDefineValidator({ "FOOLALA" }, {}));
  c.SetSemanticDefineMetaDataName("test.defs");
  c.Compile(
    "#define FOOTBALL AWESOME\n"
    "#define FOOTLOOSE TOO\n"
    "#define FOOBAR 123\n"
    "#define FOOD\n"
    "#define FOO 1 2 3\n"
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n",
    {L"/Vd"},
    { { L"FOODEF", L"1"} }
  );
  std::string disassembly = c.Disassemble();
  // Check for root named md node. It contains pointers to md nodes for each define.
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!test.defs"));
  // #define FOODEF 1
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOODEF\", !\"1\"}"));
  // #define FOOTBALL AWESOME
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOOTBALL\", !\"AWESOME\"}"));
  // #define FOOTLOOSE TOO
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOOTLOOSE\", !\"TOO\"}"));
  // #define FOOD
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOOD\", !\"\"}"));
  // #define FOO 1 2 3
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOO\", !\"1 2 3\"}"));
  // FOOBAR should be excluded.
  VERIFY_IS_TRUE(
    disassembly.npos ==
    disassembly.find("!{!\"FOOBAR\""));
}

TEST_F(ExtensionTest, DefineValidationError) {
  Compiler c(m_dllSupport);
  c.RegisterSemanticDefine(L"FOO*");
  c.SetSemanticDefineValidator(new TestSemanticDefineValidator({ "FOO" }, {}));
  IDxcOperationResult *pCompileResult = c.Compile(
    "#define FOO 1\n"
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n",
    {L"/Vd"}, {}
  );

  // Check that validation error causes compile failure.
  CheckOperationFailed(pCompileResult);
  std::string errors = GetCompileErrors(pCompileResult);
  // Check that the error message is for the validation failure.
  VERIFY_IS_TRUE(
    errors.npos !=
    errors.find("hlsl.hlsl:1:9: error: bad define: FOO"));
}

TEST_F(ExtensionTest, DefineValidationWarning) {
  Compiler c(m_dllSupport);
  c.RegisterSemanticDefine(L"FOO*");
  c.SetSemanticDefineValidator(new TestSemanticDefineValidator({}, { "FOO" }));
  IDxcOperationResult *pCompileResult = c.Compile(
    "#define FOO 1\n"
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n",
    { L"/Vd" }, {}
  );

  std::string errors = GetCompileErrors(pCompileResult);
  // Check that the error message is for the validation failure.
  VERIFY_IS_TRUE(
    errors.npos !=
    errors.find("hlsl.hlsl:1:9: warning: bad define: FOO"));

  // Check the define is still emitted.
  std::string disassembly = c.Disassemble();
  // Check for root named md node. It contains pointers to md nodes for each define.
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!hlsl.semdefs"));
  // #define FOO 1
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOO\", !\"1\"}"));
}

TEST_F(ExtensionTest, DefineNoValidatorOk) {
  Compiler c(m_dllSupport);
  c.RegisterSemanticDefine(L"FOO*");
  c.Compile(
    "#define FOO 1\n"
    "float4 main() : SV_Target {\n"
    "  return 0;\n"
    "}\n",
    { L"/Vd" }, {}
  );

  std::string disassembly = c.Disassemble();
  // Check the define is emitted.
  // #define FOO 1
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("!{!\"FOO\", !\"1\"}"));
}


TEST_F(ExtensionTest, IntrinsicWhenAvailableThenUsed) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "float2 main(float2 v : V, int2 i : I) : SV_Target {\n"
    "  test_proc(v);\n"
    "  float2 a = test_fn(v);\n"
    "  int2 b = test_fn(i);\n"
    "  return a + b;\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // Things to call out:
  // - result is float, not a vector
  // - mangled name contains the 'test' and '.r' parts
  // - opcode is first i32 argument
  // - second argument is float, ie it got scalarized
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call void @\"test.\\01?test_proc@hlsl@@YAXV?$vector@M$01@@@Z.r\"(i32 2, float"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call float @\"test.\\01?test_fn@hlsl@@YA?AV?$vector@M$01@@V2@@Z.r\"(i32 1, float"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call i32 @\"test.\\01?test_fn@hlsl@@YA?AV?$vector@H$01@@V2@@Z.r\"(i32 1, i32"));

  // - attributes are added to the declaration (the # at the end of the decl)
  //   TODO: would be nice to check for the actual attribute (e.g. readonly)
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("declare float @\"test.\\01?test_fn@hlsl@@YA?AV?$vector@M$01@@V2@@Z.r\"(i32, float) #"));
}

TEST_F(ExtensionTest, CustomIntrinsicName) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "float2 main(float2 v : V, int2 i : I) : SV_Target {\n"
    "  float2 a = test_poly(v);\n"
    "  int2   b = test_poly(i);\n"
    "  int2   c = test_int(i);\n"
    "  return a + b + c;\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // - custom name works for polymorphic function
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call float @test_poly.float(i32 3, float"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call i32 @test_poly.i32(i32 3, i32"));

  // - custom name works for non-polymorphic function
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call i32 @test_int(i32 4, i32"));
}

TEST_F(ExtensionTest, NoLowering) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "float2 main(float2 v : V, int2 i : I) : SV_Target {\n"
    "  float2 a = test_nolower(v);\n"
    "  float2 b = test_nolower(i);\n"
    "  return a + b;\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // - custom name works for non-lowered function
  // - non-lowered function has vector type as argument
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call <2 x float> @test_nolower.float(i32 5, <2 x float>"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call <2 x i32> @test_nolower.i32(i32 5, <2 x i32>"));
}

TEST_F(ExtensionTest, PackedLowering) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "float2 main(float2 v1 : V1, float2 v2 : V2, float3 v3 : V3) : SV_Target {\n"
    "  test_pack_0(v1);\n"
    "  int2   a = test_pack_1();\n"
    "  float2 b = test_pack_2(v1, v2);\n"
    "  float  c = test_pack_3(v1);\n"
    "  float2 d = test_pack_4(v3);\n"
    "  return a + b + float2(c, c);\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // - pack strategy changes vectors into structs
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call void @test_pack_0.float(i32 6, { float, float }"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call { float, float } @test_pack_1.float(i32 7)"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call { float, float } @test_pack_2.float(i32 8, { float, float }"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call float @test_pack_3.float(i32 9, { float, float }"));
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call { float, float } @test_pack_4.float(i32 10, { float, float, float }"));
}

TEST_F(ExtensionTest, ReplicateLoweringWhenOnlyVectorIsResult) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "float2 main(float2 v1 : V1, float2 v2 : V2, float3 v3 : V3) : SV_Target {\n"
    "  return test_rand();\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // - replicate strategy works for vector results
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call float @test_rand(i32 11)"));
}

TEST_F(ExtensionTest, UnsignedOpcodeIsUnchanged) {
  Compiler c(m_dllSupport);
  c.RegisterIntrinsicTable(new TestIntrinsicTable());
  c.Compile(
    "uint main(uint v1 : V1) : SV_Target {\n"
    "  return test_unsigned(v1);\n"
    "}\n",
    { L"/Vd" }, {}
  );
  std::string disassembly = c.Disassemble();

  // - opcode is unchanged when it matches an hlsl intrinsic with
  //   an unsigned version.
  // Note that 113 is the current opcode for IOP_min. If that opcode
  // changes the test will need to be updated to reflect the new opcode.
  VERIFY_IS_TRUE(
    disassembly.npos !=
    disassembly.find("call i32 @test_unsigned(i32 113, "));
}
