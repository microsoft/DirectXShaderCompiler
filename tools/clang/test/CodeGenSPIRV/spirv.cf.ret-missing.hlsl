// Run: %dxc -T vs_6_0 -E main -Wno-return-type

// CHECK:[[null:%\d+]] = OpConstantNull %float

float main(bool a: A) : B {
    if (a) return 1.0;
    // No return value for else

// CHECK:      %if_merge = OpLabel
// CHECK-NEXT: OpReturnValue [[null]]
// CHECK-NEXT: OpFunctionEnd
}
