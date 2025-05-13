#pragma once

#if HAVE_COOPVEC_API

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <cstdlib>
#include <memory>
#include <random>
#include <vector>

#include "dxc/Support/microcom.h"

#include "CoopVecAPI.h"

class LinAlgHeaderIncludeHandler : public IDxcIncludeHandler {
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
        WEX::Logging::Log::Error(
            L"Missing expected TAEF runtime parameter LinAlgHeader");
        return E_FAIL;
      }

      if (ParamValue.IsEmpty())
        return E_FAIL;

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
static std::vector<uint8_t> CreateAllOnesInputMatrix(size_t Width,
                                                     size_t Height) {
  std::vector<EltTy> InputMatrix(Width * Height);
  for (size_t i = 0; i < Width * Height; i++) {
    if constexpr (std::is_same_v<EltTy, uint8_t> ||
                  std::is_same_v<EltTy, int8_t>) {
      InputMatrix[i] = 1;
    } else if constexpr (std::is_same_v<EltTy, DirectX::PackedVector::HALF>) {
      InputMatrix[i] = ConvertFloat32ToFloat16(1.0f);
    } else if constexpr (std::is_same_v<EltTy, float>) {
      InputMatrix[i] = 1.0f;
    } else {
      VERIFY_FAIL(L"Unsupported input type");
      break;
    }
  }

  // Convert to uint8_t vector
  std::vector<uint8_t> Uint8InputMatrix(InputMatrix.size() * sizeof(EltTy));
  std::memcpy(Uint8InputMatrix.data(), InputMatrix.data(),
              InputMatrix.size() * sizeof(EltTy));
  return Uint8InputMatrix;
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
    VERIFY_FAIL(WEX::Common::String().Format(
        L"Unrecognized D3D12_LINEAR_ALGEBRA_DATATYPE: %d", DataType));
    return L"";
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
static size_t
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
    VERIFY_FAIL(L"Unsupported matrix data type");
    return 1;
  }
}

static size_t GetNumPackedElementsForInputDataType(
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
    VERIFY_FAIL(L"Unsupported input data type");
    return L"";
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
    VERIFY_FAIL(L"Unsupported interpretation");
    return L"";
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

static bool IsIntegralDataType(D3D12_LINEAR_ALGEBRA_DATATYPE DataType) {
  return DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8 ||
         DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8 ||
         DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16 ||
         DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16 ||
         DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32 ||
         DataType == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32;
}

static size_t
GetVectorElementSize(D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
                     D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation) {
  switch (DataType) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    return sizeof(int8_t);
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    return sizeof(int16_t);
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    if (DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED ||
        DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED) {
      return sizeof(int8_t);
    } else {
      return sizeof(int32_t);
    }
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    return sizeof(DirectX::PackedVector::HALF);
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
    return sizeof(float);
  default:
    VERIFY_FAIL(L"Unsupported data type");
    return 0;
  }
}

static size_t
GetMatrixElementSize(D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation) {
  switch (DataInterpretation) {
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
    // The CPU reference matrix is always int8 for all integer
    // interpretations. The GPU version will be converted to the destination
    // format by ConvertLinearAlgebraMatrix.
    return sizeof(int8_t);
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
  case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
    // The CPU reference matrix is always FP32 for all FP interpretations.
    // The GPU version will be converted to the destination format by
    // ConvertLinearAlgebraMatrix.
    return sizeof(float);
  default:
    VERIFY_FAIL(L"Unsupported data type");
    return 0;
  }
}

class TestVector {
private:
  size_t NumVectors = 0;
  size_t VectorSize = 0;
  size_t ElementSize = 0;
  size_t Stride = 0;
  size_t TotalBytes = 0;
  std::unique_ptr<uint8_t[]> Buffer;

public:
  TestVector(size_t NumVectors, size_t VectorSize, size_t ElementSize,
             size_t Alignment = 16)
      : NumVectors(NumVectors), VectorSize(VectorSize),
        ElementSize(ElementSize) {
    if (NumVectors == 0)
      VERIFY_FAIL(L"NumVectors must be greater than 0");
    if (VectorSize == 0)
      VERIFY_FAIL(L"VectorSize must be greater than 0");
    if (ElementSize == 0)
      VERIFY_FAIL(L"ElementSize must be greater than 0");

    const size_t VectorBytes = VectorSize * ElementSize;
    Stride = ((VectorBytes + Alignment - 1) / Alignment) * Alignment;
    TotalBytes = Stride * NumVectors;

    Buffer = std::make_unique<uint8_t[]>(TotalBytes);
    std::fill(Buffer.get(), Buffer.get() + TotalBytes, (uint8_t)0xFF);
  }

  // Copy constructor
  TestVector(const TestVector &other)
      : NumVectors(other.NumVectors), VectorSize(other.VectorSize),
        ElementSize(other.ElementSize), Stride(other.Stride),
        TotalBytes(other.TotalBytes) {
    if (other.Buffer) {
      Buffer = std::make_unique<uint8_t[]>(TotalBytes);
      std::memcpy(Buffer.get(), other.Buffer.get(), TotalBytes);
    }
  }

  // Move constructor
  TestVector(TestVector &&other) noexcept
      : NumVectors(other.NumVectors), VectorSize(other.VectorSize),
        ElementSize(other.ElementSize), Stride(other.Stride),
        TotalBytes(other.TotalBytes), Buffer(std::move(other.Buffer)) {
    // Reset the source object
    other.NumVectors = 0;
    other.VectorSize = 0;
    other.ElementSize = 0;
    other.Stride = 0;
    other.TotalBytes = 0;
  }

  ~TestVector() = default;

  size_t getNumVectors() const { return NumVectors; }
  size_t getVectorSize() const { return VectorSize; }
  size_t getElementSize() const { return ElementSize; }
  size_t getStride() const { return Stride; }
  size_t getTotalBytes() const { return TotalBytes; }
  uint8_t *getBuffer() { return Buffer.get(); }
  const uint8_t *getBuffer() const { return Buffer.get(); }

  // Copy assignment operator
  TestVector &operator=(const TestVector &other) {
    if (this != &other) {
      // Copy metadata
      NumVectors = other.NumVectors;
      VectorSize = other.VectorSize;
      ElementSize = other.ElementSize;
      Stride = other.Stride;
      TotalBytes = other.TotalBytes;

      // Copy data
      if (other.Buffer) {
        Buffer = std::make_unique<uint8_t[]>(TotalBytes);
        std::memcpy(Buffer.get(), other.Buffer.get(), TotalBytes);
      } else {
        Buffer.reset();
      }
    }
    return *this;
  }

  // Move assignment operator
  TestVector &operator=(TestVector &&other) noexcept {
    if (this != &other) {
      // Move metadata and buffer
      NumVectors = other.NumVectors;
      VectorSize = other.VectorSize;
      ElementSize = other.ElementSize;
      Stride = other.Stride;
      TotalBytes = other.TotalBytes;
      Buffer = std::move(other.Buffer);

      // Reset the source object
      other.NumVectors = 0;
      other.VectorSize = 0;
      other.ElementSize = 0;
      other.Stride = 0;
      other.TotalBytes = 0;
    }
    return *this;
  }

  template <typename T> T *getVector(size_t I) {
    return reinterpret_cast<T *>(Buffer.get() + I * Stride);
  }

  template <typename T> const T *getVector(size_t I) const {
    return reinterpret_cast<const T *>(Buffer.get() + I * Stride);
  }

  template <typename T> void fill(const T &Value) {
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        Vec[J] = Value;
    }
  }

  template <typename T>
  void fillSimpleTestData(D3D12_LINEAR_ALGEBRA_DATATYPE MatrixInterpretation,
                          std::mt19937 &Rnd) {
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        if constexpr (std::is_same_v<T, DirectX::PackedVector::HALF> ||
                      std::is_same_v<T, float>) {
          float Elt = 0.0f;

          // Generate random input in the following ranges:
          // - Integral types: [-3, 4] by 1
          // - FP types: [-0.5, 1] by 0.5
          if (IsIntegralDataType(MatrixInterpretation))
            Elt = static_cast<float>(Rnd() & 0x7) - 3.0f;
          else
            Elt = (static_cast<float>(Rnd() & 0x3) - 1.0f) / 2.0f;

          if constexpr (std::is_same_v<T, DirectX::PackedVector::HALF>)
            Vec[J] = static_cast<T>(ConvertFloat32ToFloat16(Elt));
          else
            Vec[J] = static_cast<T>(Elt);
        } else {
          // Generate random input in the following ranges:
          // - Signed types: [-8, 7] by 1
          // - Unsigned types: [0, 15] by 1
          if constexpr (std::is_signed_v<T>)
            Vec[J] = static_cast<T>((int32_t)(Rnd() & 0xf) - 8);
          else
            Vec[J] = static_cast<T>((uint32_t)(Rnd() & 0xf));
        }
    }
  }

  template <typename T> void FillSimpleMatrixTestData(std::mt19937 &Rnd) {
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        if constexpr (std::is_same_v<T, DirectX::PackedVector::HALF>) {
          float Elt = (static_cast<float>(Rnd() & 0x3) - 1.0f) / 2.0f;
          Vec[J] = static_cast<T>(ConvertFloat32ToFloat16(Elt));
        } else if constexpr (std::is_same_v<T, float>) {
          float Elt = (static_cast<float>(Rnd() & 0x3) - 1.0f) / 2.0f;
          Vec[J] = static_cast<T>(Elt);
        } else {
          if constexpr (std::is_signed_v<T>) {
            Vec[J] = static_cast<T>((int32_t)(Rnd() & 0xf) - 8);
          } else {
            Vec[J] = static_cast<T>((uint32_t)(Rnd() & 0xf));
          }
        }
    }
  }

  static TestVector
  createSimpleTestVector(size_t NumVectors, size_t VectorSize,
                         D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
                         D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation,
                         D3D12_LINEAR_ALGEBRA_DATATYPE MatrixInterpretation,
                         std::mt19937 &Rnd) {
    const size_t ElementSize =
        ::CoopVecHelpers::GetVectorElementSize(DataType, DataInterpretation);

    TestVector Vec(NumVectors, VectorSize, ElementSize);
    switch (DataType) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
      Vec.fillSimpleTestData<int8_t>(MatrixInterpretation, Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
      Vec.fillSimpleTestData<uint8_t>(MatrixInterpretation, Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
      Vec.fillSimpleTestData<int16_t>(MatrixInterpretation, Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
      Vec.fillSimpleTestData<uint16_t>(MatrixInterpretation, Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
      Vec.fillSimpleTestData<int32_t>(MatrixInterpretation, Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      if (DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED ||
          DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED) {
        Vec.fillSimpleTestData<uint8_t>(MatrixInterpretation, Rnd);
      } else {
        Vec.fillSimpleTestData<uint32_t>(MatrixInterpretation, Rnd);
      }
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
      Vec.fillSimpleTestData<DirectX::PackedVector::HALF>(MatrixInterpretation,
                                                          Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      Vec.fillSimpleTestData<float>(MatrixInterpretation, Rnd);
      break;
    default:
      VERIFY_FAIL(L"Unsupported data type");
      break;
    }
    return Vec;
  }

  static TestVector
  createSimpleTestMatrix(size_t NumVectors, size_t VectorSize,
                         D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation,
                         std::mt19937 &Rnd) {
    const size_t ElementSize =
        ::CoopVecHelpers::GetMatrixElementSize(DataInterpretation);

    TestVector Vec(NumVectors, VectorSize, ElementSize);
    switch (DataInterpretation) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      // The CPU reference matrix is always int8 for all integer
      // interpretations. The GPU version will be converted to the destination
      // format by ConvertLinearAlgebraMatrix.
      Vec.FillSimpleMatrixTestData<int8_t>(Rnd);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      // The CPU reference matrix is always FP32 for all FP interpretations.
      // The GPU version will be converted to the destination format by
      // ConvertLinearAlgebraMatrix.
      Vec.FillSimpleMatrixTestData<float>(Rnd);
      break;
    default:
      VERIFY_FAIL(L"Unsupported data type");
      break;
    }
    return Vec;
  }

  D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO
  getConversionInfo(ID3D12Device *D3DDevice,
                    D3D12_LINEAR_ALGEBRA_DATATYPE DestDataType,
                    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT MatrixLayout) {
    // Create source matrix info
    D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO ConvertInfo = {};
    ConvertInfo.SrcInfo.SrcDataType =
        ::CoopVecHelpers::GetMatrixSrcDataType(DestDataType);
    ConvertInfo.SrcInfo.SrcLayout =
        D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR;

    // Create destination matrix info
    ConvertInfo.DestInfo.DestSize = 0; // Will be populated by driver
    UINT DestEltSize = 0;
    switch (DestDataType) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
      ConvertInfo.DestInfo.DestDataType = D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8;
      DestEltSize = 1;
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
      ConvertInfo.DestInfo.DestDataType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
      DestEltSize = 2; // FP16
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
      ConvertInfo.DestInfo.DestDataType =
          D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3;
      DestEltSize = 1; // FP8
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
      ConvertInfo.DestInfo.DestDataType =
          D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2;
      DestEltSize = 1; // FP8
      break;
    }
    ConvertInfo.SrcInfo.SrcStride = static_cast<UINT>(getStride());
    ConvertInfo.SrcInfo.SrcSize = static_cast<UINT>(getTotalBytes());

    ConvertInfo.DestInfo.DestLayout = MatrixLayout;
    ConvertInfo.DestInfo.DestStride = 0;
    ConvertInfo.DestInfo.NumRows = static_cast<UINT>(getNumVectors());
    ConvertInfo.DestInfo.NumColumns = static_cast<UINT>(getVectorSize());

    if (MatrixLayout == D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR) {
      // Align to 16 bytes
      ConvertInfo.DestInfo.DestStride =
          (static_cast<UINT>(getVectorSize()) * DestEltSize + 15) & ~15;
    } else if (MatrixLayout ==
               D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR) {
      // Align to 16 bytes
      ConvertInfo.DestInfo.DestStride =
          (static_cast<UINT>(getNumVectors()) * DestEltSize + 15) & ~15;
    }

    // Get destination size using preview interface
    {
      CComPtr<ID3D12DevicePreview> PreviewDevice;
      VERIFY_SUCCEEDED(D3DDevice->QueryInterface(__uuidof(ID3D12DevicePreview),
                                                 (void **)&PreviewDevice));

      // Query required destination size
      PreviewDevice->GetLinearAlgebraMatrixConversionDestinationInfo(
          &ConvertInfo.DestInfo);
    }

    return ConvertInfo;
  }

  static TestVector
  matrixVectorMultiply(const TestVector &Matrix, const TestVector &InputVector,
                       const TestVector &Bias, bool HasBias,
                       D3D12_LINEAR_ALGEBRA_DATATYPE MatrixInterpretation,
                       D3D12_LINEAR_ALGEBRA_DATATYPE InputType) {
    // The CPU reference matrix is FP32 for all FP interpretations.
    bool IsMatrixFP32 = false;
    switch (MatrixInterpretation) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
      IsMatrixFP32 = true;
      break;
    default:
      break;
    }

    TestVector ResultVec(InputVector.getNumVectors(), Matrix.getNumVectors(),
                         sizeof(float));

    if (IsMatrixFP32) {
      for (size_t VecIdx = 0; VecIdx < InputVector.getNumVectors(); ++VecIdx) {
        const DirectX::PackedVector::HALF *InputBiasFP16 =
            Bias.getVector<DirectX::PackedVector::HALF>(0);
        for (size_t OutputIdx = 0; OutputIdx < Matrix.getNumVectors();
             ++OutputIdx) {
          float Acc = 0;

          for (size_t InputIdx = 0; InputIdx < Matrix.getVectorSize();
               ++InputIdx) {
            float InputElem;
            if (InputType == D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32)
              InputElem = InputVector.getVector<float>(VecIdx)[InputIdx];
            else
              InputElem = ConvertFloat16ToFloat32(
                  InputVector.getVector<DirectX::PackedVector::HALF>(
                      VecIdx)[InputIdx]);

            float const MatrixElem =
                Matrix.getVector<float>(OutputIdx)[InputIdx];
            Acc += InputElem * MatrixElem;
          }

          if (HasBias)
            Acc += ConvertFloat16ToFloat32(InputBiasFP16[OutputIdx]);

          ResultVec.getVector<float>(VecIdx)[OutputIdx] = Acc;
        }
      }
    } else if (MatrixInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8) {
      for (size_t VecIdx = 0; VecIdx < InputVector.getNumVectors(); ++VecIdx) {
        const int32_t *InputBiasI32 = Bias.getVector<int32_t>(0);
        for (size_t OutputIdx = 0; OutputIdx < Matrix.getNumVectors();
             ++OutputIdx) {
          int Acc = 0;

          for (size_t InputIdx = 0; InputIdx < Matrix.getVectorSize();
               ++InputIdx) {
            int InputElem;
            if (InputType == D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32)
              InputElem = static_cast<int>(
                  InputVector.getVector<float>(VecIdx)[InputIdx]);
            else
              InputElem = InputVector.getVector<int8_t>(VecIdx)[InputIdx];

            int const MatrixElem =
                Matrix.getVector<int8_t>(OutputIdx)[InputIdx];
            Acc += InputElem * MatrixElem;
          }

          if (HasBias)
            Acc += InputBiasI32[OutputIdx];

          ResultVec.getVector<float>(VecIdx)[OutputIdx] =
              static_cast<float>(Acc);
        }
      }
    } else {
      VERIFY_FAIL(L"Unsupported matrix interpretation");
    }

    return ResultVec;
  }
};
}; // namespace CoopVecHelpers

#endif // HAVE_COOPVEC_API
