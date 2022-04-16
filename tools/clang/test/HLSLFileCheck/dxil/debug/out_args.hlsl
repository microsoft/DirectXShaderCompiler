// RUN: %dxc -E main -T ps_6_0 %s -Zi -O0 | FileCheck %s

// CHECK-NOT: DW_OP_deref

// CHECK-DAG: call void @llvm.dbg.value(metadata <3 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00>, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"arg1" !DIExpression() func:"bar"
// CHECK-DAG: call void @llvm.dbg.value(metadata <3 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00>, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"arg0" !DIExpression() func:"foo"
// CHECK-DAG: call void @llvm.dbg.value(metadata <3 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00>, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"output" !DIExpression() func:"main"

void foo(out float3 arg0) {
  arg0 = float3(1,2,3); // @BREAK
  return;
}

void bar(inout float3 arg1) {
  arg1 += float3(1,2,3);
  return;
}

[RootSignature("")]
float3 main() : SV_Target {
  float3 output;
  foo(output);
  bar(output);
  return output;
}

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

