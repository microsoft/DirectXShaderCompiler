// Header for HLSL Linear Algebra Matrix APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 10)) &&           \
    (__HLSL_VERSION >= 2021)

#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Whlsl-groupshared-202x"

namespace hlsl {

#define SIZE_TYPE int

template <typename T> struct is_arithmetic {
  static const bool value = false;
};

#define __ARITHMETIC_TYPE(type)                                                \
  template <> struct is_arithmetic<type> {                                     \
    static const bool value = true;                                            \
  };

#if __HLSL_ENABLE_16_BIT
__ARITHMETIC_TYPE(uint16_t)
__ARITHMETIC_TYPE(int16_t)
#endif
__ARITHMETIC_TYPE(uint)
__ARITHMETIC_TYPE(int)
__ARITHMETIC_TYPE(uint64_t)
__ARITHMETIC_TYPE(int64_t)
__ARITHMETIC_TYPE(half)
__ARITHMETIC_TYPE(float)
__ARITHMETIC_TYPE(double)

template <typename T> struct is_signed {
  static const bool value = true;
};

#define __UNSIGNED_TYPE(type)                                                  \
  template <> struct is_signed<type> {                                         \
    static const bool value = false;                                           \
  };

#if __HLSL_ENABLE_16_BIT
__UNSIGNED_TYPE(uint16_t)
#endif
__UNSIGNED_TYPE(uint)
__UNSIGNED_TYPE(uint64_t)

#undef __UNSIGNED_TYPE

template <bool B, typename T> struct enable_if {};

template <typename T> struct enable_if<true, T> {
  using type = T;
};

} // namespace hlsl

namespace dxil {

// This enum must _exactly_ match the DXIL constants.
enum class ComponentType : uint32_t {
  Invalid = 0,
  I1 = 1,
  I16 = 2,
  U16 = 3,
  I32 = 4,
  U32 = 5,
  I64 = 6,
  U64 = 7,
  F16 = 8,
  F32 = 9,
  F64 = 10,
  SNormF16 = 11,
  UNormF16 = 12,
  SNormF32 = 13,
  UNormF32 = 14,
  SNormF64 = 15,
  UNormF64 = 16,
  PackedS8x32 = 17,
  PackedU8x32 = 18,

  // BEGIN NEW FOR SM 6.10
  I8 = 19,
  U8 = 20,
  F8_E4M3 = 21,
  F8_E5M2 = 22,
  // END

  LastEntry
};

} // namespace dxil

namespace dx {

namespace linalg {

#define __COMPONENT_TYPE(type) type = (uint)dxil::ComponentType::type

// This enum only defines values that are valid for Matrix component types.
// Each enumeration's value matches the cooresponding DXIL constant.
struct ComponentType {
  enum ComponentEnum {
    // Signed integers.
    __COMPONENT_TYPE(I8),
    __COMPONENT_TYPE(I16),
    __COMPONENT_TYPE(I32),
    __COMPONENT_TYPE(I64),

    // Unsigned integers.
    __COMPONENT_TYPE(U8),
    __COMPONENT_TYPE(U16),
    __COMPONENT_TYPE(U32),
    __COMPONENT_TYPE(U64),

    // Floating point types.
    __COMPONENT_TYPE(F8_E4M3),
    __COMPONENT_TYPE(F8_E5M2),
    __COMPONENT_TYPE(F16),
    __COMPONENT_TYPE(F32),
    __COMPONENT_TYPE(F64),
  };
};

#undef __COMPONENT_TYPE

using ComponentEnum = ComponentType::ComponentEnum;

struct MatrixUse {
  enum MatrixUseEnum {
    A = 0,
    B = 1,
    Accumulator = 2,
  };
};
using MatrixUseEnum = MatrixUse::MatrixUseEnum;

struct MatrixScope {
  enum MatrixScopeEnum {
    Thread = 0,
    Wave = 1,
    ThreadGroup = 2,
  };
};
using MatrixScopeEnum = MatrixScope::MatrixScopeEnum;

struct MatrixLayout {
  enum MatrixLayoutEnum {
    RowMajor = 0,
    ColMajor = 1,
    MulOptimal = 2,
    MulOptimalTranspose = 3,
    OuterProductOptimal = 4,
    OuterProductOptimalTranspose = 5,
  };
};
using MatrixLayoutEnum = MatrixLayout::MatrixLayoutEnum;

namespace __detail {
template <ComponentEnum CompTy> struct ComponentTypeTraits {
  using Type = uint;
  static const bool IsNativeScalar = false;
  static const uint ElementsPerScalar = 4;
};

#define __MATRIX_SCALAR_COMPONENT_MAPPING(enum_val, type)                      \
  template <> struct ComponentTypeTraits<enum_val> {                           \
    using Type = type;                                                         \
    static const bool IsNativeScalar = true;                                   \
    static const uint ElementsPerScalar = 1;                                   \
  };

#if __HLSL_ENABLE_16_BIT
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::I16, int16_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::U16, uint16_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::F16, float16_t)
#endif

__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::I32, int32_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::U32, uint32_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::F32, float)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::I64, int64_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::U64, uint64_t)
__MATRIX_SCALAR_COMPONENT_MAPPING(ComponentType::F64, double)

template <ComponentEnum DstTy, ComponentEnum SrcTy, int SrcN> struct DstN {
  static const int Value =
      (SrcN * ComponentTypeTraits<SrcTy>::ElementsPerScalar) /
      ComponentTypeTraits<DstTy>::ElementsPerScalar;
};

} // namespace __detail

template <ComponentEnum ElementType, uint DimA> struct VectorRef {
  ByteAddressBuffer Buf;
  uint Offset;
};

template <typename T, int N, ComponentEnum DT> struct InterpretedVector {
  vector<T, N> Data;
  static const ComponentEnum Interpretation = DT;
  static const SIZE_TYPE Size =
      __detail::ComponentTypeTraits<DT>::ElementsPerScalar * N;
};

template <ComponentEnum DT, typename T, int N>
InterpretedVector<T, N, DT> MakeInterpretedVector(vector<T, N> Vec) {
  InterpretedVector<T, N, DT> IV = {Vec};
  return IV;
}

template <ComponentEnum DestTy, ComponentEnum OriginTy, typename T, int N>
InterpretedVector<typename __detail::ComponentTypeTraits<DestTy>::Type,
                  __detail::DstN<DestTy, OriginTy, N>::Value, DestTy>
Convert(vector<T, N> Vec) {
  vector<typename __detail::ComponentTypeTraits<DestTy>::Type,
         __detail::DstN<DestTy, OriginTy, N>::Value>
      Result;
  __builtin_LinAlg_Convert(Result, Vec, OriginTy, DestTy);
  return MakeInterpretedVector<DestTy>(Result);
}

template <ComponentEnum ComponentTy, SIZE_TYPE M, SIZE_TYPE N,
          MatrixUseEnum Use, MatrixScopeEnum Scope>
class Matrix {
  using ElementType = typename __detail::ComponentTypeTraits<ComponentTy>::Type;
  // If this isn't a native scalar, we have an 8-bit type, so we have 4 elements
  // packed in each scalar value.
  static const uint ElementsPerScalar =
      __detail::ComponentTypeTraits<ComponentTy>::ElementsPerScalar;
  static const bool IsNativeScalar =
      __detail::ComponentTypeTraits<ComponentTy>::IsNativeScalar;

  using HandleT = __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(ComponentTy, M, N, Use, Scope)]];
  HandleT __handle;

  template <ComponentEnum NewCompTy, MatrixUseEnum NewUse = Use,
            bool Transpose = false>
  Matrix<NewCompTy, M, N, NewUse, Scope> Cast() {
    Matrix<NewCompTy, M, N, NewUse, Scope> Result;
    __builtin_LinAlg_CopyConvertMatrix(Result.__handle, __handle, Transpose);
    return Result;
  }

  template <typename T>
  static typename hlsl::enable_if<hlsl::is_arithmetic<T>::value, Matrix>::type
  Splat(T Val) {
    Matrix Result;
    __builtin_LinAlg_FillMatrix(Result.__handle, Val);
    return Result;
  }

  static Matrix Load(ByteAddressBuffer Res, uint StartOffset, uint Stride,
                     MatrixLayoutEnum Layout,
                     uint Align = sizeof(ElementType)) {
    Matrix Result;
    __builtin_LinAlg_MatrixLoadFromDescriptor(Result.__handle, Res, StartOffset,
                                              Stride, Layout, Align);
    return Result;
  }

  static Matrix Load(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
                     MatrixLayoutEnum Layout,
                     uint Align = sizeof(ElementType)) {
    Matrix Result;
    __builtin_LinAlg_MatrixLoadFromDescriptor(Result.__handle, Res, StartOffset,
                                              Stride, Layout, Align);
    return Result;
  }

  template <typename T, SIZE_TYPE Size>
  static typename hlsl::enable_if<hlsl::is_arithmetic<T>::value &&
                                      (M * N / ElementsPerScalar <= Size),
                                  Matrix>::type
  Load(groupshared T Arr[Size], uint StartIdx, uint Stride,
       MatrixLayoutEnum Layout) {
    Matrix Result;
    __builtin_LinAlg_MatrixLoadFromMemory(Result.__handle, Arr, StartIdx,
                                          Stride, Layout);
    return Result;
  }

  template <ComponentEnum LocalComp = ComponentTy>
  typename hlsl::enable_if<LocalComp == ComponentTy && IsNativeScalar,
                           uint>::type
  Length() {
    return __builtin_LinAlg_MatrixLength(__handle);
  }

  template <ComponentEnum LocalComp = ComponentTy>
  typename hlsl::enable_if<LocalComp == ComponentTy && IsNativeScalar,
                           uint2>::type
  GetCoordinate(uint Index) {
    return __builtin_LinAlg_MatrixGetCoordinate(__handle, Index);
  }

  template <ComponentEnum LocalComp = ComponentTy>
  typename hlsl::enable_if<LocalComp == ComponentTy && IsNativeScalar,
                           ElementType>::type
  Get(uint Index) {
    ElementType Result;
    __builtin_LinAlg_MatrixGetElement(Result, __handle, Index);
    return Result;
  }

  template <ComponentEnum LocalComp = ComponentTy>
  typename hlsl::enable_if<LocalComp == ComponentTy && IsNativeScalar,
                           void>::type
  Set(uint Index, ElementType Value) {
    __builtin_LinAlg_MatrixSetElement(__handle, __handle, Index, Value);
  }

  void Store(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
             MatrixLayoutEnum Layout, uint Align = sizeof(ElementType)) {
    __builtin_LinAlg_MatrixStoreToDescriptor(__handle, Res, StartOffset, Stride,
                                             Layout, Align);
  }

  template <typename T, SIZE_TYPE Size>
  typename hlsl::enable_if<hlsl::is_arithmetic<T>::value &&
                               (M * N / ElementsPerScalar <= Size),
                           void>::type
  Store(groupshared T Arr[Size], uint StartIdx, uint Stride,
        MatrixLayoutEnum Layout) {
    __builtin_LinAlg_MatrixStoreToMemory(__handle, Arr, StartIdx, Stride,
                                         Layout);
  }

  // Accumulate methods
  template <MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  InterlockedAccumulate(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
                        MatrixLayoutEnum Layout,
                        uint Align = sizeof(ElementType)) {
    __builtin_LinAlg_MatrixAccumulateToDescriptor(__handle, Res, StartOffset,
                                                  Stride, Layout, Align);
  }

  template <typename T, MatrixUseEnum UseLocal = Use,
            MatrixScopeEnum ScopeLocal = Scope, SIZE_TYPE Size>
  typename hlsl::enable_if<
      hlsl::is_arithmetic<T>::value && Use == MatrixUse::Accumulator &&
          UseLocal == Use && (M * N / ElementsPerScalar <= Size) &&
          Scope == MatrixScope::Wave && ScopeLocal == Scope,
      void>::type
  InterlockedAccumulate(groupshared T Arr[Size], uint StartIdx, uint Stride,
                        MatrixLayoutEnum Layout) {
    __builtin_LinAlg_MatrixAccumulateToMemory(__handle, Arr, StartIdx, Stride,
                                              Layout);
  }

  template <ComponentEnum CompTy, MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  Accumulate(const Matrix<CompTy, M, N, MatrixUse::A, Scope> MatrixA) {
    __builtin_LinAlg_MatrixAccumulate(__handle, __handle, MatrixA.__handle);
  }

  template <ComponentEnum CompTy, MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  Accumulate(const Matrix<CompTy, M, N, MatrixUse::B, Scope> MatrixB) {
    __builtin_LinAlg_MatrixAccumulate(__handle, __handle, MatrixB.__handle);
  }

  template <ComponentEnum LHSTy, ComponentEnum RHSTy, SIZE_TYPE K,
            MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  MultiplyAccumulate(const Matrix<LHSTy, M, K, MatrixUse::A, Scope> MatrixA,
                     const Matrix<RHSTy, K, N, MatrixUse::B, Scope> MatrixB) {
    __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(
        __handle, __handle, MatrixA.__handle, MatrixB.__handle);
  }
};

// Thread-scope Matrices are read-only. Using a template partial
// specialization for this simplifies the SFINAE-foo above.
template <ComponentEnum ComponentTy, SIZE_TYPE M, SIZE_TYPE N,
          MatrixUseEnum Use>
class Matrix<ComponentTy, M, N, Use, MatrixScope::Thread> {
  using ElementType = typename __detail::ComponentTypeTraits<ComponentTy>::Type;

  using HandleT = __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(
      ComponentTy, M, N, Use, MatrixScope::Thread)]];
  HandleT __handle;

  template <MatrixLayoutEnum Layout, MatrixUseEnum UseLocal = Use>
  static typename hlsl::enable_if<Use == MatrixUse::A && UseLocal == Use,
                                  Matrix>::type
  Load(ByteAddressBuffer Res, uint StartOffset, uint Stride,
       uint Align = sizeof(ElementType)) {
    Matrix Result;
    __builtin_LinAlg_MatrixLoadFromDescriptor(Result.__handle, Res, StartOffset,
                                              Stride, Layout, Align);
    return Result;
  }

  template <MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  InterlockedAccumulate(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
                        MatrixLayoutEnum Layout,
                        uint Align = sizeof(ElementType)) {
    __builtin_LinAlg_MatrixAccumulateToDescriptor(__handle, Res, StartOffset,
                                                  Stride, Layout, Align);
  }
};

MatrixUseEnum AccumulatorLayout() {
  return (MatrixUseEnum)(__builtin_LinAlg_MatrixQueryAccumulatorLayout());
}

template <ComponentEnum OutTy, ComponentEnum ATy, ComponentEnum BTy,
          SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::Wave>
Multiply(const Matrix<ATy, M, K, MatrixUse::A, MatrixScope::Wave> MatrixA,
         const Matrix<BTy, K, N, MatrixUse::B, MatrixScope::Wave> MatrixB) {
  Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::Wave> Result;
  __builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                        MatrixB.__handle);
  return Result;
}

template <ComponentEnum CompTy, SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
Matrix<CompTy, M, N, MatrixUse::Accumulator, MatrixScope::Wave>
Multiply(const Matrix<CompTy, M, K, MatrixUse::A, MatrixScope::Wave> MatrixA,
         const Matrix<CompTy, K, N, MatrixUse::B, MatrixScope::Wave> MatrixB) {
  Matrix<CompTy, M, N, MatrixUse::Accumulator, MatrixScope::Wave> Result;
  __builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                        MatrixB.__handle);
  return Result;
}

template <ComponentEnum OutTy, ComponentEnum ATy, ComponentEnum BTy,
          SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Multiply(
    const Matrix<ATy, M, K, MatrixUse::A, MatrixScope::ThreadGroup> MatrixA,
    const Matrix<BTy, K, N, MatrixUse::B, MatrixScope::ThreadGroup> MatrixB) {
  Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Result;
  __builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                        MatrixB.__handle);
  return Result;
}

template <ComponentEnum CompTy, SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
Matrix<CompTy, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Multiply(
    const Matrix<CompTy, M, K, MatrixUse::A, MatrixScope::ThreadGroup> MatrixA,
    const Matrix<CompTy, K, N, MatrixUse::B, MatrixScope::ThreadGroup>
        MatrixB) {
  Matrix<CompTy, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Result;
  __builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                        MatrixB.__handle);
  return Result;
}

// Cooperative Vector Replacement API
// Cooperative Vector operates on per-thread vectors multiplying against B
// matrices with thread scope.

template <typename OutputElTy, typename InputElTy, SIZE_TYPE M, SIZE_TYPE K,
          ComponentEnum MatrixDT>
// clang-format off
typename hlsl::enable_if<hlsl::is_arithmetic<InputElTy>::value, vector<OutputElTy, M> >::type
// clang-format on
Multiply(Matrix<MatrixDT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
         vector<InputElTy, K> Vec) {
  vector<OutputElTy, M> Result;
  __builtin_LinAlg_MatrixVectorMultiply(Result, MatrixA.__handle,
                                        hlsl::is_signed<OutputElTy>::value, Vec,
                                        MatrixDT);
  return Result;
}

template <typename OutputElTy, typename InputElTy, typename BiasElTy,
          SIZE_TYPE M, SIZE_TYPE K, ComponentEnum MatrixDT>
// clang-format off
typename hlsl::enable_if<hlsl::is_arithmetic<InputElTy>::value, vector<OutputElTy, M> >::type
// clang-format on
MultiplyAdd(Matrix<MatrixDT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            vector<InputElTy, K> Vec, vector<BiasElTy, M> Bias) {
  vector<OutputElTy, M> Result;
  __builtin_LinAlg_MatrixVectorMultiplyAdd(Result, MatrixA.__handle,
                                           hlsl::is_signed<OutputElTy>::value,
                                           Vec, MatrixDT, Bias, MatrixDT);
  return Result;
}

template <typename OutputElTy, typename InputElTy, ComponentEnum InputInterp,
          typename BiasElTy, SIZE_TYPE M, SIZE_TYPE VecK, SIZE_TYPE K,
          ComponentEnum MatrixDT>
// clang-format off
typename hlsl::enable_if<
    InterpretedVector<InputElTy, VecK, InputInterp>::Size == K,
    vector<OutputElTy, M> >::type
// clang-format on
MultiplyAdd(Matrix<MatrixDT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            InterpretedVector<InputElTy, VecK, InputInterp> InterpVec,
            vector<BiasElTy, M> Bias) {
  vector<OutputElTy, M> Result;
  __builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputElTy>::value,
      InterpVec.Data, InterpVec.Interpretation, Bias, MatrixDT);
  return Result;
}

template <typename OutputElTy, typename InputElTy, ComponentEnum BiasElTy,
          SIZE_TYPE M, SIZE_TYPE K, ComponentEnum MatrixDT>
// clang-format off
typename hlsl::enable_if<hlsl::is_arithmetic<InputElTy>::value,
                         vector<OutputElTy, M> >::type
// clang-format on
MultiplyAdd(Matrix<MatrixDT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            vector<InputElTy, K> Vec, VectorRef<BiasElTy, M> BiasRef) {
  using BiasVecTy =
      vector<typename __detail::ComponentTypeTraits<BiasElTy>::Type, M>;
  BiasVecTy BiasVec = BiasRef.Buf.template Load<BiasVecTy>(BiasRef.Offset);
  vector<OutputElTy, M> Result;
  __builtin_LinAlg_MatrixVectorMultiplyAdd(Result, MatrixA.__handle,
                                           hlsl::is_signed<OutputElTy>::value,
                                           Vec, MatrixDT, BiasVec, BiasElTy);
  return Result;
}

template <typename OutputElTy, typename InputElTy, ComponentEnum InputInterp,
          ComponentEnum BiasElTy, SIZE_TYPE M, SIZE_TYPE VecK, SIZE_TYPE K,
          ComponentEnum MatrixDT>
// clang-format off
typename hlsl::enable_if<
    InterpretedVector<InputElTy, VecK, InputInterp>::Size == K,
    vector<OutputElTy, M> >::type
// clang-format on
MultiplyAdd(Matrix<MatrixDT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            InterpretedVector<InputElTy, VecK, InputInterp> InterpVec,
            VectorRef<BiasElTy, M> BiasRef) {
  using BiasVecTy =
      vector<typename __detail::ComponentTypeTraits<BiasElTy>::Type, M>;
  BiasVecTy BiasVec = BiasRef.Buf.template Load<BiasVecTy>(BiasRef.Offset);
  vector<OutputElTy, M> Result;
  __builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputElTy>::value,
      InterpVec.Data, InterpVec.Interpretation, BiasVec, BiasElTy);
  return Result;
}

// Outer product functions
template <ComponentEnum OutTy, typename InputElTy, SIZE_TYPE M, SIZE_TYPE N>
Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::Thread>
OuterProduct(vector<InputElTy, M> VecA, vector<InputElTy, N> VecB) {
  Matrix<OutTy, M, N, MatrixUse::Accumulator, MatrixScope::Thread> Result;
  __builtin_LinAlg_MatrixOuterProduct(Result.__handle, VecA, VecB);
  return Result;
}

} // namespace linalg

} // namespace dx

#pragma dxc diagnostic pop

#endif // SM 6.10 check and HV version check
