// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// Test that debug information is correct for arrays of vectors,
// which get SROA'ed into per-element arrays, breaking the original
// contiguousness between vector elements.

// CHECK-DAG: %[[xs:.*]] = alloca [2 x i32]
// CHECK-DAG: %[[ys:.*]] = alloca [2 x i32]
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xs]], i64 0, metadata ![[divar:.*]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[xs]], i64 32, metadata ![[divar]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[ys]], i64 0, metadata ![[divar]], metadata !{{.*}})
// CHECK-DAG: call void @llvm.dbg.value(metadata [2 x i32]* %[[ys]], i64 32, metadata ![[divar]], metadata !{{.*}})

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK-DAG: ![[divar]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "val"
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 0, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 32, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 64, 32)
// CHECK-DAG: !DIExpression(DW_OP_bit_piece, 96, 32)

int main(int input : IN) : OUT
{
    int2 val[2] = (int2[2])input;
    return val[0].x;
}