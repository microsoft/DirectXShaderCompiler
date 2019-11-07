// RUN: %dxc -E main -T ps_6_0 -Zi -Od %s | FileCheck %s

// CHECK: void @llvm.dbg.value(metadata i32 %
// CHECK: void @llvm.dbg.value(metadata i32 %
// CHECK: void @llvm.dbg.value(metadata i32 %
// CHECK: void @llvm.dbg.value(metadata i32 %{{.*}}, i64 0, metadata ![[var_md:[0-9]+]], metadata ![[expr_md:[0-9]+]]

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK-DAG: ![[var_md]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "my_uv"
// CHECK-DAG: ![[expr_md]] = !DIExpression(DW_OP_bit_piece,

[RootSignature("")]
float2 main(uint2 uv : TEXCOORD) : SV_Target {
  uint2 my_uv = {
    uv.y * 0.5,
    1.0 - uv.x,
  };
  return my_uv;
}

