// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// Test that debug information is correct for nested array of aggregates,
// where SROA wreaks havok by breaking the initial variable element contiguity.

// CHECK-DAG: %[[xx:.*]] = alloca [2 x i32]
// CHECK-DAG: %[[xy:.*]] = alloca [2 x i32]
// CHECK-DAG: %[[y:.*]] = alloca [4 x float]
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xx]], i64 0, metadata ![[divar:.*]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xx]], i64 32, metadata ![[divar]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xy]], i64 0, metadata ![[divar]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xy]], i64 32, metadata ![[divar]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [4 x float]* %[[y]], i64 0, metadata ![[divar]], metadata ![[yexp0:.*]])
// CHECK-DAG: call void @llvm.dbg.value(metadata [4 x float]* %[[y]], i64 64, metadata ![[divar]], metadata ![[yexp1:.*]])

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK-DAG: ![[divar]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "val"
// i32 pieces
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 32, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 128, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 160, 32)
// float pieces
// CHECK-DAG: ![[yexp0]] = !DIExpression(DW_OP_bit_piece, 64, 64)
// CHECK-DAG: ![[yexp1]] = !DIExpression(DW_OP_bit_piece, 192, 64)

typedef struct { int2 x; float y[2]; } type[2];

int main(int input : IN) : OUT
{
    type val = (type)input;
    return val[0].x.x;
}