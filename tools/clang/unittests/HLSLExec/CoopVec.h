#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <vector>

#include "dxc/Support/microcom.h"

#include "CoopVecAPI.h"

struct LinAlgHeaderIncludeHandler : public IDxcIncludeHandler {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  dxc::DxcDllSupport &DxcSupport;

public:
  LinAlgHeaderIncludeHandler() = delete;
  LinAlgHeaderIncludeHandler(dxc::DxcDllSupport &DxcSupport)
      : m_dwRef(0), DxcSupport(DxcSupport) {}

  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename,
                                       IDxcBlob **ppIncludeSource) {
    if (wcscmp(pFilename, L"dx/linalg.h") == 0 ||
        wcscmp(pFilename, L".\\dx\\linalg.h") == 0) {
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

      CComPtr<IDxcUtils> pHeaderUtils;

      IFT(DxcSupport.CreateInstance(CLSID_DxcUtils, &pHeaderUtils));

      IDxcBlobEncoding *pHeaderBlob;
      IFT(pHeaderUtils->LoadFile(RealHeaderPath, nullptr, &pHeaderBlob));

      *ppIncludeSource = pHeaderBlob;

      return S_OK;
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
// FIXME: This is a workaround for a warning-as-error about unused parameters.
#pragma warning(push)
#pragma warning(disable : 4100)
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
#pragma warning(pop)
  }
};

struct CoopVecHelpers {
  template <typename EltTy>
  static std::vector<uint8_t> CreateAllOnesInputMatrix(uint32_t Width,
                                                       uint32_t Height) {
    std::vector<EltTy> inputMatrix(Width * Height);
    for (uint32_t i = 0; i < Width * Height; i++) {
      if constexpr (std::is_same_v<EltTy, uint8_t> ||
                    std::is_same_v<EltTy, int8_t>) {
        inputMatrix[i] = 1;
      } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
        inputMatrix[i] = ConvertFloat32ToFloat16(1.0f);
      } else if constexpr (std::is_same_v<EltTy, float>) {
        inputMatrix[i] = 1.0f;
      }
    }

    // Convert to uint8_t vector
    std::vector<uint8_t> uint8InputMatrix(inputMatrix.size() * sizeof(EltTy));
    std::memcpy(uint8InputMatrix.data(), inputMatrix.data(),
                inputMatrix.size() * sizeof(EltTy));
    return uint8InputMatrix;
  }

  template <typename EltTy>
  static std::vector<uint8_t> CreateInputVector(uint32_t NumThreads,
                                                uint32_t EltsPerThread) {
    std::vector<EltTy> inputVector(NumThreads * EltsPerThread);
    std::fill(inputVector.begin(), inputVector.end(), EltTy(0));
    if (EltsPerThread < 2) {
      WEX::Logging::Log::Error(L"EltsPerThread must be at least 2");
      return std::vector<uint8_t>();
    }
    for (uint32_t TID = 0; TID < NumThreads; TID++) {
      if constexpr (std::is_same_v<EltTy, uint8_t> ||
                    std::is_same_v<EltTy, int8_t>) {
        inputVector[TID * EltsPerThread + 0] = 1;
        inputVector[TID * EltsPerThread + 1] = 1;
      } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
        inputVector[TID * EltsPerThread + 0] = ConvertFloat32ToFloat16(1.0f);
        inputVector[TID * EltsPerThread + 1] = ConvertFloat32ToFloat16(1.0f);
      } else if constexpr (std::is_same_v<EltTy, float>) {
        inputVector[TID * EltsPerThread + 0] = 1.0f;
        inputVector[TID * EltsPerThread + 1] = 1.0f;
      } else {
        WEX::Logging::Log::Error(L"Unsupported input type");
      }
    }

    // Convert to uint8_t vector
    std::vector<uint8_t> uint8InputVector(inputVector.size() * sizeof(EltTy));
    std::memcpy(uint8InputVector.data(), inputVector.data(),
                inputVector.size() * sizeof(EltTy));
    return uint8InputVector;
  }

  template <typename EltTy>
  static std::vector<uint8_t> CreateInputBias(uint32_t NumElts) {
    std::vector<EltTy> inputBias(NumElts);
    if constexpr (std::is_same_v<EltTy, uint8_t> ||
                  std::is_same_v<EltTy, int8_t>) {
      std::fill(inputBias.begin(), inputBias.end(), EltTy(1));
    } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
      std::fill(inputBias.begin(), inputBias.end(),
                ConvertFloat32ToFloat16(1.0f));
    } else if constexpr (std::is_same_v<EltTy, int32_t>) {
      std::fill(inputBias.begin(), inputBias.end(), 1);
    } else {
      WEX::Logging::Log::Error(L"Unsupported bias type");
    }
    // Convert to uint8_t vector
    std::vector<uint8_t> uint8InputBias(inputBias.size() * sizeof(EltTy));
    std::memcpy(uint8InputBias.data(), inputBias.data(),
                inputBias.size() * sizeof(EltTy));
    return uint8InputBias;
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
    if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(
            FilterKey, ParamValue))) {
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
    if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(
            FilterKey, ParamValue))) {
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
  static std::wstring GetHlslDataTypeForDataType(
      D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
      D3D12_LINEAR_ALGEBRA_DATATYPE InputInterpretation) {
    UNREFERENCED_PARAMETER(InputInterpretation);
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

  static std::wstring GetHlslInterpretationForDataType(
      D3D12_LINEAR_ALGEBRA_DATATYPE Interpretation) {
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
};