// Header for HLSL Linear Algebra Matrix APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 10)) &&           \
    (__HLSL_VERSION >= 2021)

namespace hlsl {

#define SIZE_TYPE int

} // namespace hlsl

namespace dxil {

// This enum is be _exactly_ the DXIL constants.
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
