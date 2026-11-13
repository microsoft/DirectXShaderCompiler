// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/user_call_attributed_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Both `f` and the user function `g` it calls are annotated with
// [[dxc::AutoDiff(fwd, bwd)]]. In forward mode, the generated
// `user::ad::fwd::f` invokes `g` unqualified; C++ unqualified name lookup
// resolves it to `user::ad::fwd::g`, which the rewriter also emits, so the
// forward variant composes through the user call correctly.
//
// Backward mode creates a temporary callee context and variable, seeds g's
// generated pullback, and routes its argument gradient into f's expression.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> g(Value<float> x)
// CHECK: return (x * x);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float g(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, multiply<float>(x_expr, x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: return (g(x) + x);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = (::g(x.value) + x.value);
// CHECK: GradientContext<float> __dxc_ad_call_0_context_0 = (GradientContext<float>)0;
// CHECK: Variable<float> __dxc_ad_call_0_arg_0 = variable(__dxc_ad_call_0_context_0, x.value);
// CHECK: g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);
// CHECK: context.gradients[x.id] += __dxc_ad_call_0_arg_0.gradient(__dxc_ad_call_0_context_0);
// CHECK: context.gradients[x.id] += __dxc_ad_seed;
// CHECK: } } } // namespace user::ad::bwd

// For f(x)=x*x+x at x=2 and seed=3: primal=6 and gradient=15.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01

[[dxc::AutoDiff(fwd, bwd)]]
float g(float x) { return x * x; }

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) { return g(x) + x; }

float main(float x : A) : SV_Target { return f(x); }
