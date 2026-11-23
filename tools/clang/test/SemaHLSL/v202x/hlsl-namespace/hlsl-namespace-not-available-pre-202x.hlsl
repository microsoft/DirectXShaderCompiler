// RUN: %dxc -T lib_6_6 -HV 2016 %s -verify
// RUN: %dxc -T lib_6_6 -HV 2017 %s -verify
// RUN: %dxc -T lib_6_6 -HV 2018 %s -verify
// RUN: %dxc -T lib_6_6 -HV 2021 %s -verify

// Before HLSL 202x, using-directives may name the implicit 'hlsl' namespace,
// but intrinsics remain global and are not available as namespace members.

float UsesFunctionScope(float x) {
  using namespace hlsl;
#if __HLSL_VERSION < 2021
  // expected-warning@-2{{keyword 'using' is a HLSL 2021 feature}}
#endif
  return sin(x);
}

float UsesBlockScope(float x) {
  {
    using namespace hlsl;
#if __HLSL_VERSION < 2021
    // expected-warning@-2{{keyword 'using' is a HLSL 2021 feature}}
#endif
    x = cos(x);
  }
  return sin(x);
}

namespace N {
  using namespace hlsl;
#if __HLSL_VERSION < 2021
  // expected-warning@-2{{keyword 'using' is a HLSL 2021 feature}}
#endif

  float UsesNamespaceScope(float x) {
    return sin(x) + ::cos(x);
  }
}

using namespace hlsl;
#if __HLSL_VERSION < 2021
// expected-warning@-2{{keyword 'using' is a HLSL 2021 feature}}
#endif

// Qualified lookup must fail even before a global overload is used.
float NoQualifiedIntrinsic(float x) {
  return hlsl::tan(x); // expected-error{{no member named 'tan' in namespace 'hlsl'}}
}

// Reopening the namespace must not populate it with intrinsics.
namespace hlsl {
  float UserFunction(float x) {
    return sin(x);
  }
}

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  float a = sin(0.5);           // OK
  float b = ::sin(0.5);         // OK
  float c = hlsl::sin(0.5);     // expected-error{{no member named 'sin' in namespace 'hlsl'}}
  float d = ::hlsl::cos(0.5);   // expected-error{{no member named 'cos' in namespace 'hlsl'}}
  float e = hlsl::UserFunction(0.5);
}
