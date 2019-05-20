// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// Test that local matrices preserve debug info without optimizations

// CHECK: %[[mat:.*]] = alloca [4 x i32]
// CHECK: call void @llvm.dbg.declare(metadata [4 x i32]* %[[mat]], metadata ![[divar:.*]], metadata ![[diexpr:.*]])

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK-DAG: ![[divar]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "mat"
// CHECK-DAG: ![[diexpr]] = !DIExpression()

int2x2 cb_mat;
int main() : OUT
{
  // Initialize from CB to ensure the variable is not optimized away
  int2x2 mat = cb_mat;
  // Consume all values but return a scalar to avoid another alloca [4 x i32]
  return determinant(mat);
}