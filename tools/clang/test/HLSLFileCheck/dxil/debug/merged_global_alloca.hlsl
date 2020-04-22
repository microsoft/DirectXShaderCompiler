// RUN: %dxc %s /T ps_6_0 /Zi /Od | FileCheck %s

// CHECK: call void @llvm.dbg.value(metadata float {{.+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"global.xyzw" !DIExpression(DW_OP_bit_piece, 0, 32)

// CHECK: call void @llvm.dbg.value(metadata float {{.+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"global.xyzw" !DIExpression(DW_OP_bit_piece, 32, 32)

// CHECK: call void @llvm.dbg.value(metadata float {{.+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"global.xyzw" !DIExpression(DW_OP_bit_piece, 64, 32)

// CHECK: call void @llvm.dbg.value(metadata float {{.+}}, i64 0, metadata !{{[0-9]+}}, metadata !{{[0-9]+}}), !dbg !{{[0-9]+}} ; var:"global.xyzw" !DIExpression(DW_OP_bit_piece, 96, 32)

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

static float4 xyzw;

[RootSignature("")]
float4 main(float4 a : A, float4 b : B) : SV_Target {
  xyzw = a * 2;
  return xyzw * b;
}

