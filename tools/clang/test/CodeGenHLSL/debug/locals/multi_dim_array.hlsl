// RUN: %dxc -E main -T vs_6_0 -Zi -Od %s | FileCheck %s

// Test that flattening array dimensions preserves debug information.

// CHECK: alloca [1 x i32]
// Should have a debug value or declare
// CHECK: call void @llvm.dbg.

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

// CHECK: !DILocalVariable(tag: DW_TAG_auto_variable, name: "x"
// CHECK: !DIExpression()

int main() : OUT
{
  int x[1][1] = { 0 };
  return x[0][0];
}