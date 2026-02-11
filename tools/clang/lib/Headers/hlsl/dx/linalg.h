// Header for HLSL Linear Algebra Matrix APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 10)) &&           \
    (__HLSL_VERSION >= 2021)

namespace hlsl {

#define SIZE_TYPE int

} // namespace hlsl

namespace dx {

namespace linalg {

struct ComponentType {
  enum ComponentEnum {
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
    U8 = 19,
    I8 = 20,
    F8_E4M3 = 21,
    F8_E5M2 = 22,
  };
};
using ComponentEnum = ComponentType::ComponentEnum;

struct MatrixUse {
  enum MatrixUseEnum { A = 0, B = 1, Accumulator = 2 };
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

template <ComponentEnum ComponentTy, SIZE_TYPE M, SIZE_TYPE N,
          MatrixUseEnum Use, MatrixScopeEnum Scope>
class Matrix {
  using HandleT = __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(ComponentTy, M, N, Use, Scope)]];
  HandleT __handle;
};

} // namespace linalg

} // namespace dx

#endif // SM 6.10 check and HV version check
