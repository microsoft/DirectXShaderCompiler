// RUN: %dxc -E main -T ps_6_0 %s -Zi -O3 | FileCheck %s

// CHECK-LABEL: @main()

// CHECK: phi float [
// CHECK: phi float [
// CHECK: phi float [
// CHECK: phi float [
// CHECK: call void @llvm.dbg.value(metadata float
// CHECK: call void @llvm.dbg.value(metadata float
// CHECK: call void @llvm.dbg.value(metadata float
// CHECK: call void @llvm.dbg.value(metadata float

[RootSignature("")]
float4 main(int a : A, float4 b : B) : SV_Target {
  float4 result = b;
  [branch]
  if (a > 10)
    result *= 4;
  return result;
}

