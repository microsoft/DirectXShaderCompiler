// Header for long vector APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 9)) &&            \
    (__HLSL_VERSION >= 2021)

namespace hlsl {

// TODO: Make a dedicated file for this
template <bool B, typename T> struct enable_if {};

template <typename T> struct enable_if<true, T> {
  using type = T;
};

} // namespace hlsl

namespace hlsl {
namespace vec {

template <int Off, int OSz, typename T, int ISz>
typename hlsl::enable_if<Off + OSz <= ISz, vector<T, OSz>>::type
slice(vector<T, ISz> In) {
  vector<T, OSz> Result = (vector<T, OSz>)0;
  [unroll] for (int I = 0; I < OSz; ++I) Result[I] = In[I + Off];
  return Result;
}

template <int OSz, typename T, int ISz>
vector<T, OSz> slice(vector<T, ISz> In) {
  return slice<0, OSz, T, ISz>(In);
}

} // namespace vec
} // namespace hlsl

#endif // SM 6.9 check and HV version check
