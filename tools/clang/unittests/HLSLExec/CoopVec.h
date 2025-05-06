#pragma once

#if HAVE_COOPVEC_API

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <vector>

#include "dxc/Support/microcom.h"

#include "CoopVecAPI.h"

struct LinAlgHeaderIncludeHandler : public IDxcIncludeHandler {
private:
  DXC_MICROCOM_REF_FIELD(RefCount)
  dxc::DxcDllSupport &DxcSupport;

public:
  LinAlgHeaderIncludeHandler() = delete;
  LinAlgHeaderIncludeHandler(dxc::DxcDllSupport &DxcSupport)
      : RefCount(0), DxcSupport(DxcSupport) {}

  DXC_MICROCOM_ADDREF_RELEASE_IMPL(RefCount)

  HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR Filename,
                                       IDxcBlob **IncludeSource) {
    if (wcscmp(Filename, L"dx/linalg.h") == 0 ||
        wcscmp(Filename, L".\\dx\\linalg.h") == 0) {
      WEX::Common::String ParamValue;
      if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(
              L"LinAlgHeader", ParamValue))) {
        return E_FAIL;
      }
      if (ParamValue.IsEmpty()) {
        return E_FAIL;
      }
      LPCWSTR RealHeaderPath =
          reinterpret_cast<LPCWSTR>(ParamValue.GetBuffer());

      CComPtr<IDxcUtils> HeaderUtils;

      IFT(DxcSupport.CreateInstance(CLSID_DxcUtils, &HeaderUtils));

      IDxcBlobEncoding *HeaderBlob;
      IFT(HeaderUtils->LoadFile(RealHeaderPath, nullptr, &HeaderBlob));

      *IncludeSource = HeaderBlob;

      return S_OK;
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID IID, void **Object) override {
// FIXME: This is a workaround for a warning-as-error about unused parameters.
#pragma warning(push)
#pragma warning(disable : 4100)
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, IID, Object);
#pragma warning(pop)
  }
};

namespace CoopVecHelpers {
template <typename EltTy>
static std::vector<uint8_t> CreateAllOnesInputMatrix(uint32_t Width,
                                                     uint32_t Height) {
  std::vector<EltTy> InputMatrix(Width * Height);
  for (uint32_t i = 0; i < Width * Height; i++) {
    if constexpr (std::is_same_v<EltTy, uint8_t> ||
                  std::is_same_v<EltTy, int8_t>) {
      InputMatrix[i] = 1;
    } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
      InputMatrix[i] = ConvertFloat32ToFloat16(1.0f);
    } else if constexpr (std::is_same_v<EltTy, float>) {
      InputMatrix[i] = 1.0f;
    } else {
      WEX::Logging::Log::Error(L"Unsupported input type");
      break;
    }
  }

  // Convert to uint8_t vector
  std::vector<uint8_t> Uint8InputMatrix(InputMatrix.size() * sizeof(EltTy));
  std::memcpy(Uint8InputMatrix.data(), InputMatrix.data(),
              InputMatrix.size() * sizeof(EltTy));
  return Uint8InputMatrix;
}

template <typename EltTy>
static std::vector<uint8_t> CreateInputVector(uint32_t NumThreads,
                                              uint32_t EltsPerThread) {
  std::vector<EltTy> InputVector(NumThreads * EltsPerThread);
  std::fill(InputVector.begin(), InputVector.end(), EltTy(0));
  if (EltsPerThread < 2) {
    WEX::Logging::Log::Error(L"EltsPerThread must be at least 2");
    return std::vector<uint8_t>();
  }
  for (uint32_t TID = 0; TID < NumThreads; TID++) {
    if constexpr (std::is_same_v<EltTy, uint8_t> ||
                  std::is_same_v<EltTy, int8_t>) {
      InputVector[TID * EltsPerThread + 0] = 1;
      InputVector[TID * EltsPerThread + 1] = 1;
    } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
      InputVector[TID * EltsPerThread + 0] = ConvertFloat32ToFloat16(1.0f);
      InputVector[TID * EltsPerThread + 1] = ConvertFloat32ToFloat16(1.0f);
    } else if constexpr (std::is_same_v<EltTy, float>) {
      InputVector[TID * EltsPerThread + 0] = 1.0f;
      InputVector[TID * EltsPerThread + 1] = 1.0f;
    } else {
      WEX::Logging::Log::Error(L"Unsupported input type");
      break;
    }
  }

  // Convert to uint8_t vector
  std::vector<uint8_t> Uint8InputVector(InputVector.size() * sizeof(EltTy));
  std::memcpy(Uint8InputVector.data(), InputVector.data(),
              InputVector.size() * sizeof(EltTy));
  return Uint8InputVector;
}

template <typename EltTy>
static std::vector<uint8_t> CreateInputBias(uint32_t NumElts) {
  std::vector<EltTy> InputBias(NumElts);
  if constexpr (std::is_same_v<EltTy, uint8_t> ||
                std::is_same_v<EltTy, int8_t>) {
    std::fill(InputBias.begin(), InputBias.end(), EltTy(1));
  } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
    std::fill(InputBias.begin(), InputBias.end(),
              ConvertFloat32ToFloat16(1.0f));
  } else if constexpr (std::is_same_v<EltTy, int32_t>) {
    std::fill(InputBias.begin(), InputBias.end(), 1);
  } else {
    WEX::Logging::Log::Error(L"Unsupported bias type");
  }
  // Convert to uint8_t vector
  std::vector<uint8_t> Uint8InputBias(InputBias.size() * sizeof(EltTy));
  std::memcpy(Uint8InputBias.data(), InputBias.data(),
              InputBias.size() * sizeof(EltTy));
  return Uint8InputBias;
}

static std::wstring
DataTypeToFilterString(D3D12_LINEAR_ALGEBRA_DATATYPE DataType) {
  switch (DataType) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
    return L"SINT8_T4_PACKED";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
    return L"UINT8_T4_PACKED";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    return L"SINT8";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    return L"UINT8";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    return L"SINT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    return L"UINT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    return L"SINT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    return L"UINT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
    return L"FLOAT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    return L"FLOAT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    return L"FLOAT_E4M3";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    return L"FLOAT_E5M2";
  default:
    return L"<UNKNOWN>";
  }
}

static bool IsDataTypeInFilter(const wchar_t *FilterKey,
                               D3D12_LINEAR_ALGEBRA_DATATYPE DataType) {
  WEX::Common::String ParamValue;
  if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(FilterKey,
                                                                ParamValue))) {
    // Filter not set, so treat as no filter
    return true;
  }
  if (ParamValue.IsEmpty()) {
    // Empty filter, so treat as no filter
    return true;
  }

  // Check if the filter matches the target data type
  LPCWSTR FilterString = reinterpret_cast<LPCWSTR>(ParamValue.GetBuffer());
  return DataTypeToFilterString(DataType) == FilterString;
}

static std::wstring
MatrixLayoutToFilterString(D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT MatrixLayout) {
  switch (MatrixLayout) {
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR:
    return L"ROW_MAJOR";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR:
    return L"COLUMN_MAJOR";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_MUL_OPTIMAL:
    return L"MUL_OPTIMAL";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL:
    return L"OUTER_PRODUCT_OPTIMAL";
  default:
    return L"<UNKNOWN>";
  }
}

static bool
IsMatrixLayoutInFilter(const wchar_t *FilterKey,
                       D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT MatrixLayout) {
  WEX::Common::String ParamValue;
  if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(FilterKey,
                                                                ParamValue))) {
    // Filter not set, so treat as no filter
    return true;
  }
  if (ParamValue.IsEmpty()) {
    // Empty filter, so treat as no filter
    return true;
  }

  // Check if the filter matches the target data type
  LPCWSTR FilterString = reinterpret_cast<LPCWSTR>(ParamValue.GetBuffer());
  return MatrixLayoutToFilterString(MatrixLayout) == FilterString;
}

static std::wstring MatrixLayoutToHlslLayoutString(
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT MatrixLayout) {
  switch (MatrixLayout) {
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR:
    return L"MATRIX_LAYOUT_ROW_MAJOR";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR:
    return L"MATRIX_LAYOUT_COLUMN_MAJOR";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_MUL_OPTIMAL:
    return L"MATRIX_LAYOUT_MUL_OPTIMAL";
  case D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL:
    return L"MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL";
  default:
    return L"<UNKNOWN>";
  }
}

// This multiplier is used to compute the row/column stride for a matrix
// given it's element size.
static int
GetStrideMultiplierForMatrixDataType(D3D12_LINEAR_ALGEBRA_DATATYPE DataType) {
  switch (DataType) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    return 1;
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    return 2;
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    return 4;
  default:
    WEX::Logging::Log::Error(L"Unsupported matrix data type");
    return 1;
  }
}

static int GetNumPackedElementsForInputDataType(
    D3D12_LINEAR_ALGEBRA_DATATYPE InputInterpretation) {
  // Int8 packed types are the only ones that have more than 1 element per
  // shader variable
  switch (InputInterpretation) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
    return 4;
  default:
    return 1;
  }
}

// This type is used in generated HLSL source to represent the vector type
// for the given data type.
static std::wstring
GetHlslDataTypeForDataType(D3D12_LINEAR_ALGEBRA_DATATYPE DataType) {
  switch (DataType) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    return L"int16_t";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    return L"uint16_t";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    return L"int32_t";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    return L"uint32_t";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    return L"half";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
    return L"float";
  default:
    WEX::Logging::Log::Error(L"Unsupported input data type");
    return L"<UNKNOWN>";
  }
}

static std::wstring
GetHlslInterpretationForDataType(D3D12_LINEAR_ALGEBRA_DATATYPE Interpretation) {
  switch (Interpretation) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
    return L"DATA_TYPE_SINT8_T4_PACKED";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
    return L"DATA_TYPE_UINT8_T4_PACKED";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    return L"DATA_TYPE_SINT8";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    return L"DATA_TYPE_UINT8";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    return L"DATA_TYPE_SINT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    return L"DATA_TYPE_UINT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    return L"DATA_TYPE_SINT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    return L"DATA_TYPE_UINT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    return L"DATA_TYPE_FLOAT16";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
    return L"DATA_TYPE_FLOAT32";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    return L"DATA_TYPE_FLOAT8_E4M3";
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    return L"DATA_TYPE_FLOAT8_E5M2";
  default:
    WEX::Logging::Log::Error(L"Unsupported interpretation");
    return L"<UNKNOWN>";
  }
}

// The returned data type is used for matrix conversion. It is hard-coded
// for the test framework where all integer matrices start as SINT8 and
// all FP matrices start as FLOAT32.
static D3D12_LINEAR_ALGEBRA_DATATYPE
GetMatrixSrcDataType(D3D12_LINEAR_ALGEBRA_DATATYPE MatrixInterpretation) {
  switch (MatrixInterpretation) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8;
  default:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
  }
}
}; // namespace CoopVecHelpers

#endif // HAVE_COOPVEC_API
