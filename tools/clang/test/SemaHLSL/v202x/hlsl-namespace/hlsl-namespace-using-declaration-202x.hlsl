// RUN: %dxc -T lib_6_3 -HV 202x %s -verify
// expected-no-diagnostics

// A using-declaration should expose an intrinsic family before any overload
// has otherwise been materialized.
using hlsl::sin;

float UseImportedScalar(float value) { return sin(value); }
float3 UseImportedVector(float3 value) { return sin(value); }
float UseGlobalImport(float value) { return ::sin(value); }

namespace math {
using hlsl::cos;
}

float UseReexportedScalar(float value) { return math::cos(value); }
float3 UseReexportedVector(float3 value) { return math::cos(value); }

namespace transitive {
using math::cos;
}

float UseTransitiveImport(float value) { return transitive::cos(value); }

namespace overloadBeforeImport {
float tan() { return 42; }
using hlsl::tan;

float UseBothOverloads(float value) { return tan() + tan(value); }
}

namespace overloadAfterImport {
using hlsl::sqrt;
float sqrt() { return 42; }

float UseBothOverloads(float value) { return sqrt() + sqrt(value); }
}
