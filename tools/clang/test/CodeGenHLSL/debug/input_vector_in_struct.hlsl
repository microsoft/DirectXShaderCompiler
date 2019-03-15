// RUN: %dxc -E main -T vs_6_0 -Zi %s | FileCheck %s

// Test that debug offsets are correct after lowering a vector
// input to loadInput calls, when the vector input itself was
// offset in a struct.

// CHECK: %[[val:.*]] = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 1, i32 undef)
// CHECK: call void @llvm.dbg.value(metadata i32 %[[val]], i64 0, metadata ![[var:.*]], metadata ![[expr:.*]])
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 %[[val]])
// CHECK: ret void

// CHECK: ![[var]] = !DILocalVariable(tag: DW_TAG_arg_variable, name: "s", arg: 1
// CHECK: ![[expr]] = !DIExpression(DW_OP_bit_piece, 96, 32)

struct S { int2 a, b; };
int main(S s : IN) : OUT { return s.b.y; }