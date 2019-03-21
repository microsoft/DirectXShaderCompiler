// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// Test that debug information is correct for arrays of structs,
// which get SROA'ed into per-element arrays, breaking the original
// contiguousness between struct elements.

// CHECK-DAG: %[[ints:.*]] = alloca [2 x i32]
// CHECK-DAG: %[[floats:.*]] = alloca [2 x float]
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[ints]], i64 0, metadata ![[divar:.*]], metadata ![[expr0:.*]])
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[ints]], i64 32, metadata ![[divar]], metadata ![[expr2:.*]])
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x float]* %[[floats]], i64 0, metadata ![[divar]], metadata ![[expr1:.*]])
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x float]* %[[floats]], i64 32, metadata ![[divar]], metadata ![[expr3:.*]])

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK-DAG: ![[divar]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "val"
// CHECK-DAG: ![[expr0]] = !DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK-DAG: ![[expr1]] = !DIExpression(DW_OP_bit_piece, 32, 32)
// CHECK-DAG: ![[expr2]] = !DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK-DAG: ![[expr3]] = !DIExpression(DW_OP_bit_piece, 96, 32)

typedef struct { int x; float y; } type[2];

int main(int input : IN) : OUT
{
    type val = (type)input;
    return val[0].x;
}