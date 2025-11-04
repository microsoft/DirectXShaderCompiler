// Header for enable_if APIs.

#if ((__SHADER_TARGET_MAJOR > 6) ||                                            \
     (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 9)) &&            \
    (__HLSL_VERSION >= 2021)

namespace hlsl {

template <bool B, typename T> struct enable_if {};

template <typename T> struct enable_if<true, T> {
  using type = T;
};

} // namespace hlsl

#endif
