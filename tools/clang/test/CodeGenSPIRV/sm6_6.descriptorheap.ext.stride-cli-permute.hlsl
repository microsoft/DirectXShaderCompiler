// Verifies: every legal CLI stride value flows independently to each heap's literal ArrayStride 
//  (full 6x6 cross product, RS!=SS proves no cross-contamination), and invalid values are rejected at option-parse time.
//
// [[RS]] = resource-heap stride, [[SS]] = sampler-heap stride (per-RUN via -D).

// ---- Full 6x6 cross product of {8,16,32,64,128,256} x {8,16,32,64,128,256} ----
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=8   -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=8   -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=8   -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=8   -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=8   -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 8   -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=8   -DSS=256
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=16  -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=16  -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=16  -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=16  -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=16  -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 16  -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=16  -DSS=256
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=32  -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=32  -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=32  -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=32  -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=32  -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 32  -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=32  -DSS=256
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=64  -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=64  -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=64  -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=64  -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=64  -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64  -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=64  -DSS=256
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=128 -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=128 -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=128 -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=128 -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=128 -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=128 -DSS=256
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 8   -spirv %s | FileCheck %s -DRS=256 -DSS=8
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s -DRS=256 -DSS=16
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 32  -spirv %s | FileCheck %s -DRS=256 -DSS=32
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 64  -spirv %s | FileCheck %s -DRS=256 -DSS=64
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 128 -spirv %s | FileCheck %s -DRS=256 -DSS=128
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 256 -fvk-sampler-heap-stride 256 -spirv %s | FileCheck %s -DRS=256 -DSS=256

// ---- Invalid values are rejected at option-parsing time (power-of-two in [8,256]) ----
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 48    -spirv %s 2>&1 | FileCheck %s --check-prefix=BADRS
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 4     -spirv %s 2>&1 | FileCheck %s --check-prefix=BADRS
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 512   -spirv %s 2>&1 | FileCheck %s --check-prefix=BADRS
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 0     -spirv %s 2>&1 | FileCheck %s --check-prefix=BADRS
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-sampler-heap-stride 24 -spirv %s 2>&1 | FileCheck %s --check-prefix=BADSS
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride abc   -spirv %s 2>&1 | FileCheck %s --check-prefix=BADNUM

// ---- Both flags invalid: both errors must be reported (no short-circuit) ----
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 48 -fvk-sampler-heap-stride 24 -spirv %s 2>&1 | FileCheck %s --check-prefix=BOTH

// Bind each heap's runtime array to its element type so the strides are checked
// independently. The resource Texture2D image array carries [[RS]]; the sampler
// array carries [[SS]].
// CHECK-DAG:   %[[TexType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:  %[[SampType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:  %[[TexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexType]]{{$}}
// CHECK-DAG: %[[SampArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SampType]]{{$}}
// CHECK-DAG:                                OpDecorate %[[TexArray]]  ArrayStride [[RS]]
// CHECK-DAG:                                OpDecorate %[[SampArray]] ArrayStride [[SS]]

// BADRS: -fvk-resource-heap-stride must be a power of 2 between 8 and 256 (inclusive)
// BADSS: -fvk-sampler-heap-stride must be a power of 2 between 8 and 256 (inclusive)
// BADNUM: invalid -fvk-resource-heap-stride argument: 'abc'
// Both flags invalid: resource error appears first, sampler error follows on next line.
// BOTH: -fvk-resource-heap-stride must be a power of 2 between 8 and 256 (inclusive)
// BOTH: -fvk-sampler-heap-stride must be a power of 2 between 8 and 256 (inclusive)

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    Texture2D<float4> tex  = ResourceDescriptorHeap[0];
    SamplerState      samp = SamplerDescriptorHeap[0];
    float4 c = tex.SampleLevel(samp, float2(0, 0), 0);
    outputBytes.Store(0, asuint(c.x));
}
