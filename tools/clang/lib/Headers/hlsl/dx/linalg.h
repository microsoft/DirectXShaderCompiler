//===----------------------------------------------------------------------===//
//
// Part of the DirectXShaderCompiler, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// DirectX Shader Model 6.10 Linear Algebra objects and APIs.
//===----------------------------------------------------------------------===//

#include <enable_if>
#include <type_traits>

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 10)) &&           \
    (__HLSL_VERSION >= 2021)

#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Whlsl-groupshared-202x"

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

  // BEGIN NEW FOR SM 6.9
  I8 = 19,
  U8 = 20,
  F8_E4M3FN = 21,
  F8_E5M2 = 22,
  // END

  // BEGIN NEW FOR SM 6.10
  BFloat16 = 23,
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
    __COMPONENT_TYPE(F8_E4M3FN),
    __COMPONENT_TYPE(F8_E5M2),
    __COMPONENT_TYPE(F16),
    __COMPONENT_TYPE(F32),
    __COMPONENT_TYPE(F64),
    __COMPONENT_TYPE(BFloat16),
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
template <ComponentEnum> struct ComponentTypeTraits {
  using Type = uint;
  static const bool IsNativeScalar = false;
  static const uint ElementsPerScalar = 4;
};

template <typename> struct TypeTraits {
  static const ComponentEnum CompType =
      (ComponentEnum)dxil::ComponentType::Invalid;
};

template <ComponentEnum> struct IsComponentTypeAvailable {
  static const bool value = true;
};

#if !__HLSL_ENABLE_16_BIT
template <> struct IsComponentTypeAvailable<ComponentType::I16> {
  static const bool value = false;
};
template <> struct IsComponentTypeAvailable<ComponentType::U16> {
  static const bool value = false;
};
template <> struct IsComponentTypeAvailable<ComponentType::F16> {
  static const bool value = false;
};
#endif

template <ComponentEnum CT, typename Ty> struct IsCompatibleVectorElement {
  static const bool IsPackedCarrier =
      hlsl::is_same<Ty, uint8_t4_packed>::value ||
      hlsl::is_same<Ty, int8_t4_packed>::value;
  static const bool value =
      IsComponentTypeAvailable<CT>::value &&
      (hlsl::is_same<Ty, typename ComponentTypeTraits<CT>::Type>::value ||
       (!ComponentTypeTraits<CT>::IsNativeScalar && IsPackedCarrier));
};

template <> struct ComponentTypeTraits<ComponentType::BFloat16> {
  using Type = uint;
  static const bool IsNativeScalar = false;
  static const uint ElementsPerScalar = 2;
};

#define __MATRIX_SCALAR_COMPONENT_MAPPING(enum_val, type)                      \
  template <> struct ComponentTypeTraits<enum_val> {                           \
    using Type = type;                                                         \
    static const bool IsNativeScalar = true;                                   \
    static const uint ElementsPerScalar = 1;                                   \
  };                                                                           \
  template <> struct TypeTraits<type> {                                        \
    static const ComponentEnum CompType = enum_val;                            \
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

template <ComponentEnum DstCT, ComponentEnum SrcCT, int SrcN> struct DstN {
  // Make sure to round up in case SrcN isn't an even multiple of the number of
  // elements per scalar
  static const int Value =
      (SrcN * ComponentTypeTraits<SrcCT>::ElementsPerScalar +
       ComponentTypeTraits<DstCT>::ElementsPerScalar - 1) /
      ComponentTypeTraits<DstCT>::ElementsPerScalar;
};

template <SIZE_TYPE MVal, SIZE_TYPE NVal, bool Transposed> struct DimMN {
  static const SIZE_TYPE M = MVal;
  static const SIZE_TYPE N = NVal;
};

template <SIZE_TYPE MVal, SIZE_TYPE NVal> struct DimMN<MVal, NVal, true> {
  static const SIZE_TYPE M = NVal;
  static const SIZE_TYPE N = MVal;
};

template <ComponentEnum CT, SIZE_TYPE PackedComponentCount>
struct ScalarCountFromPackedComponents {
  static const SIZE_TYPE ElementsPerScalar =
      ComponentTypeTraits<CT>::ElementsPerScalar;
  static const SIZE_TYPE Value =
      (PackedComponentCount + ElementsPerScalar - 1) / ElementsPerScalar;
};

template <ComponentEnum CT, SIZE_TYPE M, SIZE_TYPE N> struct DefaultAlign {
  enum {
    MinDim = M < N ? M : N,
    ScalarCount = ScalarCountFromPackedComponents<CT, MinDim>::Value,
    ByteAlign = ScalarCount * 4,
    MinByteAlign = ByteAlign < 4 ? 4 : ByteAlign,
    Value = MinByteAlign < 16 ? MinByteAlign : 16
  };
};

} // namespace __detail

template <ComponentEnum CT, uint DimA> struct VectorRef {
  ByteAddressBuffer Buf;
  uint Offset;
};

template <typename Ty, int N, ComponentEnum CT> struct InterpretedVector {
  vector<Ty, N> Data;
  static const ComponentEnum Interpretation = CT;
  static const SIZE_TYPE Size =
      __detail::ComponentTypeTraits<CT>::ElementsPerScalar * N;
};

template <ComponentEnum CT, typename Ty, int N>
typename hlsl::enable_if< __detail::IsCompatibleVectorElement<CT, Ty>::value,
                          InterpretedVector<Ty, N, CT> >::type
MakeInterpretedVector(vector<Ty, N> Vec) {
  InterpretedVector<Ty, N, CT> IV = {Vec};
  return IV;
}

template <ComponentEnum DestCT, ComponentEnum OriginCT, typename Ty, int N>
typename hlsl::enable_if<
    DestCT != OriginCT && __detail::IsComponentTypeAvailable<DestCT>::value &&
        __detail::IsCompatibleVectorElement<OriginCT, Ty>::value,
    InterpretedVector<typename __detail::ComponentTypeTraits<DestCT>::Type,
                      __detail::DstN<DestCT, OriginCT, N>::Value,
                      DestCT> >::type
Convert(vector<Ty, N> Vec) {
  vector<typename __detail::ComponentTypeTraits<DestCT>::Type,
         __detail::DstN<DestCT, OriginCT, N>::Value>
      Result;
  dx::__builtin_LinAlg_Convert(Result, Vec, OriginCT, DestCT);
  return MakeInterpretedVector<DestCT>(Result);
}

template <ComponentEnum DestCT, ComponentEnum OriginCT, typename Ty, int N>
typename hlsl::enable_if<
    DestCT == OriginCT &&
        __detail::IsCompatibleVectorElement<OriginCT, Ty>::value,
    InterpretedVector<Ty, N, DestCT> >::type
Convert(vector<Ty, N> Vec) {
  return MakeInterpretedVector<DestCT>(Vec);
}

template <ComponentEnum CT, SIZE_TYPE M, SIZE_TYPE N, MatrixUseEnum Use,
          MatrixScopeEnum Scope>
class Matrix {
  using ElementType = typename __detail::ComponentTypeTraits<CT>::Type;
  // If this isn't a native scalar, we have a type that may pack more than 1
  // element in each scalar value. (Ex. 8bit => 4elems, 16bit => 2elems)
  static const uint ElementsPerScalar =
      __detail::ComponentTypeTraits<CT>::ElementsPerScalar;
  static const bool IsNativeScalar =
      __detail::ComponentTypeTraits<CT>::IsNativeScalar;

  using HandleT = __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(CT, M, N, Use, Scope)]];
  HandleT __handle;

  template <ComponentEnum NewCT, MatrixUseEnum NewUse = Use,
            bool Transpose = false>
  [[nodiscard]] Matrix<NewCT, __detail::DimMN<M, N, Transpose>::M,
                       __detail::DimMN<M, N, Transpose>::N, NewUse, Scope>
  Cast() {
    Matrix<NewCT, __detail::DimMN<M, N, Transpose>::M,
           __detail::DimMN<M, N, Transpose>::N, NewUse, Scope>
        Result;
    dx::__builtin_LinAlg_CopyConvertMatrix(Result.__handle, __handle,
                                           Transpose);
    return Result;
  }

  template <typename Ty>
  [[nodiscard]] static
      typename hlsl::enable_if<hlsl::is_arithmetic<Ty>::value, Matrix>::type
      Splat(Ty Val) {
    Matrix Result;
    dx::__builtin_LinAlg_FillMatrix(Result.__handle, hlsl::is_signed<Ty>::value,
                                    Val);
    return Result;
  }

  template <uint Align = __detail::DefaultAlign<CT, M, N>::Value>
  [[nodiscard]] static Matrix Load(ByteAddressBuffer Res, uint StartOffset,
                                   uint Stride, MatrixLayoutEnum Layout) {
    Matrix Result;
    dx::__builtin_LinAlg_MatrixLoadFromDescriptor(
        Result.__handle, Res, StartOffset, Stride, Layout, Align);
    return Result;
  }

  template <uint Align = __detail::DefaultAlign<CT, M, N>::Value>
  [[nodiscard]] static Matrix Load(RWByteAddressBuffer Res, uint StartOffset,
                                   uint Stride, MatrixLayoutEnum Layout) {
    Matrix Result;
    dx::__builtin_LinAlg_MatrixLoadFromDescriptor(
        Result.__handle, Res, StartOffset, Stride, Layout, Align);
    return Result;
  }

  template <typename Ty, SIZE_TYPE Size>
  [[nodiscard]] static typename hlsl::enable_if<
      (hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                     ElementType>::value ||
       hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                     uint8_t4_packed>::value),
      Matrix>::type
  Load(groupshared Ty Arr[Size], uint StartIdx, uint Stride,
       MatrixLayoutEnum Layout) {
    Matrix Result;
    dx::__builtin_LinAlg_MatrixLoadFromMemory(Result.__handle, Arr, StartIdx,
                                              Stride, Layout);
    return Result;
  }

  template <ComponentEnum LocalCT = CT>
  typename hlsl::enable_if<LocalCT == CT && IsNativeScalar, uint>::type
  Length() {
    return dx::__builtin_LinAlg_MatrixLength(__handle);
  }

  template <ComponentEnum LocalCT = CT>
  typename hlsl::enable_if<LocalCT == CT && IsNativeScalar, uint2>::type
  GetCoordinate(uint Index) {
    return dx::__builtin_LinAlg_MatrixGetCoordinate(__handle, Index);
  }

  template <ComponentEnum LocalCT = CT>
  typename hlsl::enable_if<LocalCT == CT && IsNativeScalar, ElementType>::type
  Get(uint Index) {
    ElementType Result;
    dx::__builtin_LinAlg_MatrixGetElement(Result, __handle, Index);
    return Result;
  }

  template <ComponentEnum LocalCT = CT>
  typename hlsl::enable_if<LocalCT == CT && IsNativeScalar, void>::type
  Set(uint Index, ElementType Value) {
    dx::__builtin_LinAlg_MatrixSetElement(__handle, __handle, Index, Value);
  }

  template <uint Align = __detail::DefaultAlign<CT, M, N>::Value>
  void Store(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
             MatrixLayoutEnum Layout) {
    dx::__builtin_LinAlg_MatrixStoreToDescriptor(__handle, Res, StartOffset,
                                                 Stride, Layout, Align);
  }

  template <typename Ty, SIZE_TYPE Size>
  typename hlsl::enable_if<
      (hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                     ElementType>::value ||
       hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                     uint8_t4_packed>::value),
      void>::type
  Store(groupshared Ty Arr[Size], uint StartIdx, uint Stride,
        MatrixLayoutEnum Layout) {
    dx::__builtin_LinAlg_MatrixStoreToMemory(__handle, Arr, StartIdx, Stride,
                                             Layout);
  }

  // Accumulate methods
  template <uint Align = __detail::DefaultAlign<CT, M, N>::Value,
            MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  InterlockedAccumulate(RWByteAddressBuffer Res, uint StartOffset, uint Stride,
                        MatrixLayoutEnum Layout) {
    dx::__builtin_LinAlg_MatrixAccumulateToDescriptor(
        __handle, Res, StartOffset, Stride, Layout, Align);
  }

  template <typename Ty, MatrixUseEnum UseLocal = Use, SIZE_TYPE Size>
  typename hlsl::enable_if<
      hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                    ElementType>::value &&
          hlsl::is_arithmetic_vector<Ty>::value &&
          Use == MatrixUse::Accumulator && UseLocal == Use,
      void>::type
  InterlockedAccumulate(groupshared Ty Arr[Size], uint StartIdx, uint Stride,
                        MatrixLayoutEnum Layout) {
    dx::__builtin_LinAlg_MatrixAccumulateToMemory(__handle, Arr, StartIdx,
                                                  Stride, Layout);
  }

  template <typename Ty, MatrixUseEnum UseLocal = Use, SIZE_TYPE Size>
  typename hlsl::enable_if<
      hlsl::is_same<typename hlsl::strip_vector_type<Ty>::type,
                    uint8_t4_packed>::value &&
          Use == MatrixUse::Accumulator && UseLocal == Use,
      void>::type
  InterlockedAccumulate(groupshared Ty Arr[Size], uint StartIdx, uint Stride,
                        MatrixLayoutEnum Layout) {
    dx::__builtin_LinAlg_MatrixAccumulateToMemory(__handle, Arr, StartIdx,
                                                  Stride, Layout);
  }

  template <ComponentEnum MatrixCT, MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  Accumulate(const Matrix<MatrixCT, M, N, MatrixUse::A, Scope> MatrixA) {
    dx::__builtin_LinAlg_MatrixAccumulate(__handle, __handle, MatrixA.__handle);
  }

  template <ComponentEnum MatrixCT, MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  Accumulate(const Matrix<MatrixCT, M, N, MatrixUse::B, Scope> MatrixB) {
    dx::__builtin_LinAlg_MatrixAccumulate(__handle, __handle, MatrixB.__handle);
  }

  template <ComponentEnum LHSCT, ComponentEnum RHSCT, SIZE_TYPE K,
            MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  MultiplyAccumulate(const Matrix<LHSCT, M, K, MatrixUse::A, Scope> MatrixA,
                     const Matrix<RHSCT, K, N, MatrixUse::B, Scope> MatrixB) {
    dx::__builtin_LinAlg_MatrixMatrixMultiplyAccumulate(
        __handle, MatrixA.__handle, MatrixB.__handle, __handle);
  }
};

// Thread-scope Matrices are read-only. Using a template partial
// specialization for this simplifies the SFINAE-foo above.
template <ComponentEnum CT, SIZE_TYPE M, SIZE_TYPE N, MatrixUseEnum Use>
class Matrix<CT, M, N, Use, MatrixScope::Thread> {
  using ElementType = typename __detail::ComponentTypeTraits<CT>::Type;

  using HandleT = __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(CT, M, N, Use, MatrixScope::Thread)]];
  HandleT __handle;

  template <MatrixLayoutEnum Layout, uint Align = 128,
            MatrixUseEnum UseLocal = Use>
  [[nodiscard]] static
      typename hlsl::enable_if<Use == MatrixUse::A && UseLocal == Use,
                               Matrix>::type
      Load(ByteAddressBuffer Res, uint StartOffset, uint Stride) {
    Matrix Result;
    dx::__builtin_LinAlg_MatrixLoadFromDescriptor(
        Result.__handle, Res, StartOffset, Stride, Layout, Align);
    return Result;
  }

  template <uint Align = 128, MatrixUseEnum UseLocal = Use>
  typename hlsl::enable_if<Use == MatrixUse::Accumulator && UseLocal == Use,
                           void>::type
  InterlockedAccumulate(RWByteAddressBuffer Res, uint StartOffset) {
    dx::__builtin_LinAlg_MatrixAccumulateToDescriptor(
        __handle, Res, StartOffset, 0, MatrixLayout::OuterProductOptimal,
        Align);
  }
};

MatrixUseEnum AccumulatorLayout() {
  return (MatrixUseEnum)(dx::__builtin_LinAlg_MatrixQueryAccumulatorLayout());
}

template <ComponentEnum OutCT, ComponentEnum ACT, ComponentEnum BCT,
          SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
[[nodiscard]] Matrix<OutCT, M, N, MatrixUse::Accumulator, MatrixScope::Wave>
Multiply(const Matrix<ACT, M, K, MatrixUse::A, MatrixScope::Wave> MatrixA,
         const Matrix<BCT, K, N, MatrixUse::B, MatrixScope::Wave> MatrixB) {
  Matrix<OutCT, M, N, MatrixUse::Accumulator, MatrixScope::Wave> Result;
  dx::__builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                            MatrixB.__handle);
  return Result;
}

template <ComponentEnum CT, SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
[[nodiscard]] Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::Wave>
Multiply(const Matrix<CT, M, K, MatrixUse::A, MatrixScope::Wave> MatrixA,
         const Matrix<CT, K, N, MatrixUse::B, MatrixScope::Wave> MatrixB) {
  Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::Wave> Result;
  dx::__builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                            MatrixB.__handle);
  return Result;
}

template <ComponentEnum OutCT, ComponentEnum ACT, ComponentEnum BCT,
          SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
[[nodiscard]] Matrix<OutCT, M, N, MatrixUse::Accumulator,
                     MatrixScope::ThreadGroup>
Multiply(
    const Matrix<ACT, M, K, MatrixUse::A, MatrixScope::ThreadGroup> MatrixA,
    const Matrix<BCT, K, N, MatrixUse::B, MatrixScope::ThreadGroup> MatrixB) {
  Matrix<OutCT, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Result;
  dx::__builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                            MatrixB.__handle);
  return Result;
}

template <ComponentEnum CT, SIZE_TYPE M, SIZE_TYPE N, SIZE_TYPE K>
[[nodiscard]] Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup>
Multiply(
    const Matrix<CT, M, K, MatrixUse::A, MatrixScope::ThreadGroup> MatrixA,
    const Matrix<CT, K, N, MatrixUse::B, MatrixScope::ThreadGroup> MatrixB) {
  Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::ThreadGroup> Result;
  dx::__builtin_LinAlg_MatrixMatrixMultiply(Result.__handle, MatrixA.__handle,
                                            MatrixB.__handle);
  return Result;
}

// Cooperative Vector Replacement API
// Cooperative Vector operates on per-thread vectors multiplying against B
// matrices with thread scope.

template <typename OutputTy, typename InputTy, SIZE_TYPE M, SIZE_TYPE K,
          ComponentEnum CT>
typename hlsl::enable_if<hlsl::is_arithmetic<InputTy>::value,
                         vector<OutputTy, M> >::type
Multiply(Matrix<CT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
         vector<InputTy, K> Vec) {
  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiply(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value, Vec,
      __detail::TypeTraits<InputTy>::CompType);
  return Result;
}

template <typename OutputTy, typename InputTy, ComponentEnum InputCT,
          SIZE_TYPE M, SIZE_TYPE K, SIZE_TYPE VecK, ComponentEnum MatrixCT>
typename hlsl::enable_if<
    InterpretedVector<InputTy, VecK, InputCT>::Size == K &&
        __detail::IsCompatibleVectorElement<InputCT, InputTy>::value,
    vector<OutputTy, M> >::type
Multiply(Matrix<MatrixCT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
         InterpretedVector<InputTy, VecK, InputCT> InterpVec) {
  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiply(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value,
      InterpVec.Data, InterpVec.Interpretation);
  return Result;
}

template <typename OutputTy, typename InputTy, typename BiasTy, SIZE_TYPE M,
          SIZE_TYPE K, ComponentEnum CT>
typename hlsl::enable_if<hlsl::is_arithmetic<InputTy>::value &&
                             hlsl::is_arithmetic<BiasTy>::value,
                         vector<OutputTy, M> >::type
MultiplyAdd(Matrix<CT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            vector<InputTy, K> Vec, vector<BiasTy, M> Bias) {

  InterpretedVector<OutputTy, M, __detail::TypeTraits<OutputTy>::CompType>
      BiasConvInterp = Convert<__detail::TypeTraits<OutputTy>::CompType,
                               __detail::TypeTraits<BiasTy>::CompType>(Bias);

  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value, Vec,
      __detail::TypeTraits<InputTy>::CompType, BiasConvInterp.Data);
  return Result;
}

template <typename OutputTy, typename InputTy, ComponentEnum InputCT,
          typename BiasTy, SIZE_TYPE M, SIZE_TYPE K, SIZE_TYPE VecK,
          ComponentEnum MatrixCT>
typename hlsl::enable_if<
    VecK == __detail::ScalarCountFromPackedComponents<InputCT, K>::Value &&
        __detail::IsCompatibleVectorElement<InputCT, InputTy>::value &&
        hlsl::is_arithmetic<BiasTy>::value,
    vector<OutputTy, M> >::type
MultiplyAdd(Matrix<MatrixCT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            InterpretedVector<InputTy, VecK, InputCT> InterpVec,
            vector<BiasTy, M> Bias) {

  InterpretedVector<OutputTy, M, __detail::TypeTraits<OutputTy>::CompType>
      BiasConvInterp = Convert<__detail::TypeTraits<OutputTy>::CompType,
                               __detail::TypeTraits<BiasTy>::CompType>(Bias);

  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value,
      InterpVec.Data, InterpVec.Interpretation, BiasConvInterp.Data);
  return Result;
}

template <typename OutputTy, typename InputTy, ComponentEnum BiasCT,
          SIZE_TYPE M, SIZE_TYPE K, ComponentEnum MatrixCT>
typename hlsl::enable_if<hlsl::is_arithmetic<InputTy>::value,
                         vector<OutputTy, M> >::type
MultiplyAdd(Matrix<MatrixCT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            vector<InputTy, K> Vec, VectorRef<BiasCT, M> BiasRef) {

  using BiasVecTy =
      vector<typename __detail::ComponentTypeTraits<BiasCT>::Type,
             __detail::ScalarCountFromPackedComponents<BiasCT, M>::Value>;
  BiasVecTy Bias = BiasRef.Buf.template Load<BiasVecTy>(BiasRef.Offset);

  // Convert currently does not support packed type vector sizes that
  // are not a multiple of the number of elements per scalar, so we
  // need to do an extra conversion here to get it into the right shape.
  // For example, if BiasRef is F8_E4M3FN and M is 7, it gets loaded into
  // vector<uint, 2>, and if OutputTy is half, Convert will return
  // vector<half, 8> instead of vector<half, 7>.
  // https://github.com/microsoft/DirectXShaderCompiler/issues/8418
  //
  // Convert to OutputTy vector with padding
  using BiasConvInterpPaddedTy = InterpretedVector<
      OutputTy,
      __detail::DstN<
          __detail::TypeTraits<OutputTy>::CompType, BiasCT,
          __detail::ScalarCountFromPackedComponents< BiasCT, M>::Value>::Value,
      __detail::TypeTraits<OutputTy>::CompType>;

  BiasConvInterpPaddedTy BiasConvInterpPadded =
      Convert<__detail::TypeTraits<OutputTy>::CompType, BiasCT>(Bias);

  // Truncate the vector to the correct size M
  vector<OutputTy, M> BiasConv = (vector<OutputTy, M>)BiasConvInterpPadded.Data;

  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value, Vec,
      __detail::TypeTraits<InputTy>::CompType, BiasConv);
  return Result;
}

template <typename OutputTy, typename InputTy, ComponentEnum InputCT,
          ComponentEnum BiasCT, SIZE_TYPE M, SIZE_TYPE K, SIZE_TYPE VecK,
          ComponentEnum MatrixCT>
typename hlsl::enable_if<
    VecK == __detail::ScalarCountFromPackedComponents<InputCT, K>::Value &&
        __detail::IsCompatibleVectorElement<InputCT, InputTy>::value,
    vector<OutputTy, M> >::type
MultiplyAdd(Matrix<MatrixCT, M, K, MatrixUse::A, MatrixScope::Thread> MatrixA,
            InterpretedVector<InputTy, VecK, InputCT> InterpVec,
            VectorRef<BiasCT, M> BiasRef) {
  using BiasVecTy =
      vector<typename __detail::ComponentTypeTraits<BiasCT>::Type,
             __detail::ScalarCountFromPackedComponents<BiasCT, M>::Value>;
  BiasVecTy Bias = BiasRef.Buf.template Load<BiasVecTy>(BiasRef.Offset);

  // Convert currently does not support packed type vector sizes that
  // are not a multiple of the number of elements per scalar, so we
  // need to do an extra conversion here to get it into the right shape.
  // For example, if BiasRef is F8_E4M3FN and M is 7, it gets loaded into
  // vector<uint, 2>, and if OutputTy is half, Convert will return
  // vector<half, 8> instead of vector<half, 7>.
  // https://github.com/microsoft/DirectXShaderCompiler/issues/8418
  //
  // Convert to OutputTy vector with padding
  using BiasConvInterpPaddedTy = InterpretedVector<
      OutputTy,
      __detail::DstN<
          __detail::TypeTraits<OutputTy>::CompType, BiasCT,
          __detail::ScalarCountFromPackedComponents< BiasCT, M>::Value>::Value,
      __detail::TypeTraits<OutputTy>::CompType>;

  BiasConvInterpPaddedTy BiasConvInterpPadded =
      Convert<__detail::TypeTraits<OutputTy>::CompType, BiasCT>(Bias);

  // Truncate the vector to the correct size M
  vector<OutputTy, M> BiasConv = (vector<OutputTy, M>)BiasConvInterpPadded.Data;

  vector<OutputTy, M> Result;
  dx::__builtin_LinAlg_MatrixVectorMultiplyAdd(
      Result, MatrixA.__handle, hlsl::is_signed<OutputTy>::value,
      InterpVec.Data, InterpVec.Interpretation, BiasConv);
  return Result;
}

// Outer product functions
template <ComponentEnum CT, typename InputTy, SIZE_TYPE M, SIZE_TYPE N>
[[nodiscard]] typename hlsl::enable_if<
    hlsl::is_arithmetic<InputTy>::value,
    Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::Thread> >::type
OuterProduct(vector<InputTy, M> VecA, vector<InputTy, N> VecB) {
  Matrix<CT, M, N, MatrixUse::Accumulator, MatrixScope::Thread> Result;
  dx::__builtin_LinAlg_MatrixOuterProduct(
      Result.__handle, hlsl::is_signed<InputTy>::value, VecA, VecB);
  return Result;
}

template <uint Align = 64, typename InputTy, SIZE_TYPE M>
typename hlsl::enable_if<hlsl::is_arithmetic<InputTy>::value, void>::type
InterlockedAccumulate(RWByteAddressBuffer Res, uint StartOffset,
                      vector<InputTy, M> Vec) {
  dx::__builtin_LinAlg_VectorAccumulateToDescriptor(Res, StartOffset, Align,
                                                    Vec);
}

} // namespace linalg

} // namespace dx

#pragma dxc diagnostic pop

#endif // SM 6.10 check and HV version check
