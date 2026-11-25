// RUN: %dxc -T vs_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK-NOT: OpUndef

Texture2D texA;
Texture2D texB;

// CHECK: %select1 = OpFunction
Texture2D select1(bool lhs) {
  if (lhs)
    return texA;
  else
    return texB;
  // no return for dead branch.
// CHECK: OpUnreachable
// CHECK-NEXT: OpFunctionEnd
}

// CHECK: %select2 = OpFunction
Texture2D select2(bool lhs) {
  if (lhs)
    return texA;
  else
    return texB;
  // no return for dead branch.
// CHECK: OpUnreachable
// CHECK-NEXT: OpFunctionEnd
}

float main(bool a: A) : B {
    Texture2D x = select1(true);
    Texture2D y = select2(true);
    return 1.0;
}

