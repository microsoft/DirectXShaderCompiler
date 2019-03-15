// RUN: %dxc -E main -T vs_6_0 -Zi %s | FileCheck %s

// Test that an input vector's debug information is preserved,
// especially through its lowering to per-element loadInputs.

// CHECK: %[[xval:.*]] = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: call void @llvm.dbg.value(metadata i32 %[[xval]], i64 0, metadata ![[var:.*]], metadata ![[xexpr:.*]])
// CHECK: %[[yval:.*]] = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 1, i32 undef)
// CHECK: call void @llvm.dbg.value(metadata i32 %[[yval]], i64 0, metadata ![[var]], metadata ![[yexpr:.*]])
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %[[xval]])
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 %[[yval]])
// CHECK: ret void

// CHECK: ![[var]] = !DILocalVariable(tag: DW_TAG_arg_variable, name: "a", arg: 1
// CHECK: ![[xexpr]] = !DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK: ![[yexpr]] = !DIExpression(DW_OP_bit_piece, 32, 32)

int2 main(int2 a : IN) : OUT { return a; }