// RUN: %dxc -E main -T vs_6_0 %s

// TODO: support struct with resource.

// without also putting them in a static assertion

// __decltype is the GCC way of saying 'decltype', but doesn't require C++11
#ifdef VERIFY_FXC
#endif

// To test with the classic compiler, run
// %sdxroot%\tools\x86\fxc.exe /T vs_6_0 scalar-operators.hlsl
// with vs_2_0 (the default) min16float usage produces a complaint that it's not supported

struct f3_s    { float3 f3; };
struct mixed_s { float3 f3; SamplerState s; };
SamplerState g_SamplerState;
f3_s    g_f3_s;
mixed_s g_mixed_s;

float4 main(float4 param4  : FOO )  : FOO {
    bool        bools       = 0;
    int         ints        = 0;
    float       floats      = 0;
    SamplerState SamplerStates = g_SamplerState;
    f3_s f3_ss = g_f3_s;
    mixed_s mixed_ss = g_mixed_s;

  // when dealing with built-in types, (SamplerState in these examples) are now 'operator
  // cannot be used with built-in type 'SamplerState'' or

  // some 'int or unsigned int type required' become 'scalar, vector, or matrix expected'

  // some 'cannot implicitly convert from 'SamplerState' to 'bool'' become 'type mismatch'

  // This confuses the semantic checks on template types, which are assumed to be only for built-in template-like constructs.
  // This confuses the semantic checks on template types, which are assumed to be only for built-in template-like constructs.
  return 1.2;
};
