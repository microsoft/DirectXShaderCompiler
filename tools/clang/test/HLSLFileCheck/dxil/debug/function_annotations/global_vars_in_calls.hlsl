// RUN: %dxc -E main -T ps_6_0 %s -Od /Zi | FileCheck %s

static float g_foo = 5;

float foo(float arg) {
  return arg * g_foo;
}

// CHECK: dx.function
float main(float a : A) : SV_Target {

  // CHECK: call void @llvm.dbg.value(metadata float 5.000000e+00, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"g_foo" !DIExpression()
  // CHECK: call void @llvm.dbg.value(metadata float 5.000000e+00, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"g_foo" !DIExpression()

  // CHECK: dx.function

  g_foo = a;
  // CHECK: call void @llvm.dbg.value(metadata float %{{[0-9]+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"g_foo" !DIExpression()
  // CHECK: call void @llvm.dbg.value(metadata float %{{[0-9]+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"g_foo" !DIExpression()

  float x = 10;

  float y = foo(x);
  // CHECK: dx.function
  // CHECK-SAME: line:6

  return y;
}


// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}



