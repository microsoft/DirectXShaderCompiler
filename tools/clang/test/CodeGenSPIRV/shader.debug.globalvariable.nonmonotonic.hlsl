// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -fcgl %s -spirv | FileCheck %s

uniform float highReg : register(c10);
uniform float lowReg  : register(c1);

float4 main() : SV_Target {
    return float4(highReg, lowReg, 0, 1);
}

// The composite must be large enough to cover the highest-register field
// (c10 -> offset 160 bytes -> +4 bytes = 164 bytes -> 1312 bits), not just
// the last-declared field's end (c1 -> offset 16 bytes + 4 bytes = 20 bytes
// -> 160 bits, which is what the old code produced).
//
// The DebugTypeComposite `Size` operand is the last-but-two operand before
// the parent CU and linkage-name; we pin an OpConstant that must equal or
// exceed 1312 bits. The simplest FileCheck for that is to pin the constant
// value directly:
//
// CHECK-DAG: [[size:%[_A-Za-z0-9]+]] = OpConstant {{%[_A-Za-z0-9]+}} 1312
// CHECK:     OpExtInst {{%[_A-Za-z0-9]+}} {{%[_A-Za-z0-9]+}} DebugTypeComposite {{.*}} [[size]] {{.*}}
