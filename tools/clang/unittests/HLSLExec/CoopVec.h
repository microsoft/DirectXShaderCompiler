#pragma once

#if HAVE_COOPVEC_API

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <cstdlib>
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

struct TestVector {
private:
  size_t NumVectors = 0;
  size_t VectorSize = 0;
  size_t ElementSize = 0;
  size_t Stride = 0;
  size_t TotalBytes = 0;
  uint8_t *Buffer = nullptr;

public:
  TestVector(size_t NumVectors, size_t VectorSize, size_t ElementSize,
             size_t Alignment = 16)
      : NumVectors(NumVectors), VectorSize(VectorSize),
        ElementSize(ElementSize) {
    if (NumVectors == 0) {
      throw std::invalid_argument("NumVectors must be greater than 0");
    }
    if (VectorSize == 0) {
      throw std::invalid_argument("VectorSize must be greater than 0");
    }
    if (ElementSize == 0) {
      throw std::invalid_argument("ElementSize must be greater than 0");
    }

    size_t VectorBytes = VectorSize * ElementSize;
    Stride = ((VectorBytes + Alignment - 1) / Alignment) * Alignment;
    TotalBytes = Stride * NumVectors;

    void *Ptr = nullptr;
#ifdef _MSC_VER
    Ptr = _aligned_malloc(TotalBytes, Alignment);
#else
    Ptr = std::aligned_alloc(Alignment, TotalBytes);
#endif
    Buffer = reinterpret_cast<uint8_t *>(Ptr);
    std::fill(Buffer, Buffer + TotalBytes, (uint8_t)0xFF);
  }

  // Copy constructor
  TestVector(const TestVector &other)
      : NumVectors(other.NumVectors), VectorSize(other.VectorSize),
        ElementSize(other.ElementSize), Stride(other.Stride),
        TotalBytes(other.TotalBytes) {

    void *Ptr = nullptr;
#ifdef _MSC_VER
    Ptr = _aligned_malloc(TotalBytes, 16);
#else
    Ptr = std::aligned_alloc(16, TotalBytes);
#endif
    Buffer = reinterpret_cast<uint8_t *>(Ptr);

    if (other.Buffer) {
      std::memcpy(Buffer, other.Buffer, TotalBytes);
    }
  }

  // Move constructor
  TestVector(TestVector &&other) noexcept
      : NumVectors(other.NumVectors), VectorSize(other.VectorSize),
        ElementSize(other.ElementSize), Stride(other.Stride),
        TotalBytes(other.TotalBytes), Buffer(other.Buffer) {

    // Reset the source object
    other.NumVectors = 0;
    other.VectorSize = 0;
    other.ElementSize = 0;
    other.Stride = 0;
    other.TotalBytes = 0;
    other.Buffer = nullptr;
  }

  ~TestVector() {
    if (Buffer) {
#ifdef _MSC_VER
      _aligned_free(Buffer);
#else
      std::free(Buffer);
#endif
    }
  }

  size_t getNumVectors() const { return NumVectors; }
  size_t getVectorSize() const { return VectorSize; }
  size_t getElementSize() const { return ElementSize; }
  size_t getStride() const { return Stride; }
  size_t getTotalBytes() const { return TotalBytes; }
  uint8_t *getBuffer() { return Buffer; }
  const uint8_t *getBuffer() const { return Buffer; }

  // Copy assignment operator
  TestVector &operator=(const TestVector &other) {
    if (this != &other) {
      // Free existing buffer
      if (Buffer) {
#ifdef _MSC_VER
        _aligned_free(Buffer);
#else
        std::free(Buffer);
#endif
        Buffer = nullptr;
      }

      // Copy metadata
      NumVectors = other.NumVectors;
      VectorSize = other.VectorSize;
      ElementSize = other.ElementSize;
      Stride = other.Stride;
      TotalBytes = other.TotalBytes;

      // Allocate new buffer
      void *Ptr = nullptr;
#ifdef _MSC_VER
      Ptr = _aligned_malloc(TotalBytes, 16);
#else
      Ptr = std::aligned_alloc(16, TotalBytes);
#endif
      Buffer = reinterpret_cast<uint8_t *>(Ptr);

      // Copy data
      if (other.Buffer) {
        std::memcpy(Buffer, other.Buffer, TotalBytes);
      }
    }
    return *this;
  }

  // Move assignment operator
  TestVector &operator=(TestVector &&other) noexcept {
    if (this != &other) {
      // Free existing buffer
      if (Buffer) {
#ifdef _MSC_VER
        _aligned_free(Buffer);
#else
        std::free(Buffer);
#endif
      }

      // Move metadata and buffer
      NumVectors = other.NumVectors;
      VectorSize = other.VectorSize;
      ElementSize = other.ElementSize;
      Stride = other.Stride;
      TotalBytes = other.TotalBytes;
      Buffer = other.Buffer;

      // Reset the source object
      other.NumVectors = 0;
      other.VectorSize = 0;
      other.ElementSize = 0;
      other.Stride = 0;
      other.TotalBytes = 0;
      other.Buffer = nullptr;
    }
    return *this;
  }

  template <typename T> T *getVector(size_t I) {
    uint8_t *Ptr = Buffer + I * Stride;
    return reinterpret_cast<T *>(Ptr);
  }

  template <typename T> const T *getVector(size_t I) const {
    const uint8_t *Ptr = Buffer + I * Stride;
    return reinterpret_cast<const T *>(Ptr);
  }

  template <typename T> void fill(const T &Value) {
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        Vec[J] = Value;
    }
  }

  template <typename T> void fillSimpleTestData() {
    // Create a vector of (1, 1, 0, ...)
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        if constexpr (std::is_same_v<T, DirectX::PackedVector::HALF>) {
          // Special case for HALF, which requires conversion from float
          Vec[J] = static_cast<T>(
              ConvertFloat32ToFloat16((J == 0 || J == 1) ? 1.0f : 0.0f));
        } else {
          Vec[J] = static_cast<T>((J == 0 || J == 1) ? 1 : 0);
        }
    }
  }

  template <typename T> void fillAllOnesTestData() {
    // Create a vector of (1, 1, 1, ...)
    for (size_t I = 0; I < NumVectors; ++I) {
      T *Vec = getVector<T>(I);
      for (size_t J = 0; J < VectorSize; ++J)
        if constexpr (std::is_same_v<T, DirectX::PackedVector::HALF>) {
          // Special case for HALF, which requires conversion from float
          Vec[J] = static_cast<T>(ConvertFloat32ToFloat16(1.0f));
        } else {
          Vec[J] = static_cast<T>(1);
        }
    }
  }

  static TestVector
  createSimpleTestVector(size_t NumVectors, size_t VectorSize,
                         D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
                         D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation) {
    size_t ElementSize;
    switch (DataType) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
      ElementSize = sizeof(int8_t);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
      ElementSize = sizeof(int16_t);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      if (DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED ||
          DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED) {
        ElementSize = sizeof(int8_t);
      } else {
        ElementSize = sizeof(int32_t);
      }
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
      ElementSize = sizeof(DirectX::PackedVector::HALF);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      ElementSize = sizeof(float);
      break;
    default:
      throw std::invalid_argument("Unsupported data type");
    }
    TestVector Vec(NumVectors, VectorSize, ElementSize);
    switch (DataType) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
      Vec.fillSimpleTestData<int8_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
      Vec.fillSimpleTestData<uint8_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
      Vec.fillSimpleTestData<int16_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
      Vec.fillSimpleTestData<uint16_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
      Vec.fillSimpleTestData<int32_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      if (DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED ||
          DataInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED) {
        Vec.fillSimpleTestData<uint8_t>();
      } else {
        Vec.fillSimpleTestData<uint32_t>();
      }
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
      Vec.fillSimpleTestData<DirectX::PackedVector::HALF>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      Vec.fillSimpleTestData<float>();
      break;
    default:
      throw std::invalid_argument("Unsupported data type");
    }
    return Vec;
  }

  static TestVector
  createAllOnesTestMatrix(size_t NumVectors, size_t VectorSize,
                          D3D12_LINEAR_ALGEBRA_DATATYPE DataInterpretation) {
    size_t ElementSize;
    switch (DataInterpretation) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      ElementSize = sizeof(int8_t);
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      ElementSize = sizeof(float);
      break;
    default:
      throw std::invalid_argument("Unsupported data type");
    }
    TestVector Vec(NumVectors, VectorSize, ElementSize);
    switch (DataInterpretation) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
      Vec.fillAllOnesTestData<int8_t>();
      break;
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
      Vec.fillAllOnesTestData<float>();
      break;
    default:
      throw std::invalid_argument("Unsupported data type");
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
    int DestEltSize = 0;
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
    ConvertInfo.SrcInfo.SrcStride = (UINT)getStride();
    ConvertInfo.SrcInfo.SrcSize = (UINT)getTotalBytes();

    ConvertInfo.DestInfo.DestLayout = MatrixLayout;
    ConvertInfo.DestInfo.DestStride = 0;
    ConvertInfo.DestInfo.NumRows = (UINT)getNumVectors();
    ConvertInfo.DestInfo.NumColumns = (UINT)getVectorSize();

    if (MatrixLayout == D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR) {
      ConvertInfo.DestInfo.DestStride = (UINT)getVectorSize() * DestEltSize;
    } else if (MatrixLayout ==
               D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR) {
      ConvertInfo.DestInfo.DestStride = (UINT)getNumVectors() * DestEltSize;
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
    bool IsFP32 = false;
    switch (MatrixInterpretation) {
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
    case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
      IsFP32 = true;
      break;
    default:
      break;
    }

    TestVector ResultVec(InputVector.getNumVectors(), Matrix.getNumVectors(),
                         sizeof(float));

    if (IsFP32) {
      for (int VecIdx = 0; VecIdx < InputVector.getNumVectors(); ++VecIdx) {
        const DirectX::PackedVector::HALF *InputBiasFP16 =
            Bias.getVector<DirectX::PackedVector::HALF>(0);
        for (int OutputIdx = 0; OutputIdx < Matrix.getNumVectors();
             ++OutputIdx) {
          float Acc = 0;

          for (int InputIdx = 0; InputIdx < Matrix.getVectorSize();
               ++InputIdx) {
            float InputElem;
            if (InputType == D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32) {
              InputElem = InputVector.getVector<float>(VecIdx)[InputIdx];
            } else {
              InputElem = ConvertFloat16ToFloat32(
                  InputVector.getVector<DirectX::PackedVector::HALF>(
                      VecIdx)[InputIdx]);
            }
            float const MatrixElem =
                Matrix.getVector<float>(OutputIdx)[InputIdx];
            Acc += InputElem * MatrixElem;
          }

          if (HasBias) {
            Acc += ConvertFloat16ToFloat32(InputBiasFP16[OutputIdx]);
          }

          float Result = Acc;
          ResultVec.getVector<float>(VecIdx)[OutputIdx] = Result;
        }
      }
    } else if (MatrixInterpretation == D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8) {
      for (int VecIdx = 0; VecIdx < InputVector.getNumVectors(); ++VecIdx) {
        const int32_t *InputBiasI32 = Bias.getVector<int32_t>(0);
        for (int OutputIdx = 0; OutputIdx < Matrix.getNumVectors();
             ++OutputIdx) {
          int Acc = 0;

          for (int InputIdx = 0; InputIdx < Matrix.getVectorSize();
               ++InputIdx) {
            int InputElem;
            if (InputType == D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32) {
              InputElem = (int)InputVector.getVector<float>(VecIdx)[InputIdx];
            } else {
              InputElem = InputVector.getVector<int8_t>(VecIdx)[InputIdx];
            }
            int const MatrixElem =
                Matrix.getVector<int8_t>(OutputIdx)[InputIdx];
            Acc += InputElem * MatrixElem;
          }

          if (HasBias) {
            Acc += InputBiasI32[OutputIdx];
          }

          float Result = float(Acc);
          ResultVec.getVector<float>(VecIdx)[OutputIdx] = Result;
        }
      }
    } else {
      throw std::invalid_argument("Unsupported matrix interpretation");
    }

    return ResultVec;
  }
};
}; // namespace CoopVecHelpers

#endif // HAVE_COOPVEC_API
