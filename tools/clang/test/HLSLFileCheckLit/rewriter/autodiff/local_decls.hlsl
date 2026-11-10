// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/local_decls_bwd_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Local variable declarations inside the differentiated function body are
// preserved as Value<T> declarations in forward mode. Backward mode expands
// their immutable bindings into the returned expression graph.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x, Value<float> y)
// CHECK: Value<float> a = (x * y);
// CHECK: Value<float> b = (a + sin(x));
// CHECK: return b;
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, Variable<float> y, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: VariableExpr<float> y_expr = makeVariableExpr<float>(y);
// CHECK-NOT: Variable<float> a
// CHECK-NOT: Variable<float> b
// CHECK: return compute_gradients_seeded(context, add<float>(multiply<float>(x_expr, y_expr), sinExpr<float>(x_expr)), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// At x=2 and y=3: f=xy+sin(x), df/dx=y+cos(x), df/dy=x.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 0x401BA31EE0000000
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 0x4004ABBB40000000
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.000000e+00

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x, float y) {
  float a = x * y;
  float b = a + sin(x);
  return b;
}

float main(float x : A) : SV_Target { return f(x, x); }
