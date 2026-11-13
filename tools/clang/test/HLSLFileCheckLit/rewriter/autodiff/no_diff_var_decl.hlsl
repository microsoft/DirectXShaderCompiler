// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// [[dxc::NoDiff]] applied to a block containing a variable declaration.
// HLSL parses leading attributes on declarations through ParseDeclaration,
// which drops statement attributes that aren't decl attributes, so the
// attribute is applied to the surrounding block instead. Every expression in
// that block is lowered through primal values.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: Value<float> a = Value<float>::CreateValue((x.value * x.value));
// CHECK: Value<float> b = Value<float>::CreateValue((a.value + x.value));
// CHECK: return Value<float>::CreateValue(b.value);
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: context.zeroGradients();
// CHECK: return ((x.value * x.value) + x.value);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  [[dxc::NoDiff]] {
    float a = x * x;
    float b = a + x;
    return b;
  }
}

float main(float x : A) : SV_Target { return f(x); }
