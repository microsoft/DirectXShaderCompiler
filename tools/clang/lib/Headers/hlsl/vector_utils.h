// Header for long vector APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 9)) &&            \
    (__HLSL_VERSION >= 2021)

#include <enable_if.h>

namespace hlsl {

// clang-format off
template <int Off, int OSz, typename T, int ISz>
typename hlsl::enable_if<Off + OSz <= ISz, vector<T, OSz> >::type
slice(vector<T, ISz> In) {
  vector<T, OSz> Result = (vector<T, OSz>)0;

  [unroll]
  for (int I = 0; I < OSz; ++I)
    Result[I] = In[I + Off];

  return Result;
}
// clang-format on

template <int OSz, typename T, int ISz>
vector<T, OSz> slice(vector<T, ISz> In) {
  return slice<0, OSz, T, ISz>(In);
}

} // namespace hlsl

#endif
