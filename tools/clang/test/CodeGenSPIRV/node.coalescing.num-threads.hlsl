// RUN: %dxc -spirv -Od -T lib_6_8 external %s | FileCheck %s

// Coalescing launch node with thread group defined in the shader

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node008_coalescing_numthreads_shader()
{
}

// CHECK: OpEntryPoint GLCompute [[SHADER:%[0-9A-Za-z_]*]]
// CHECK: OpExecutionMode [[SHADER]] CoalescingAMDX
// CHECK: OpExecutionMode [[SHADER]] LocalSize 1024 1 1
// CHECK: OpReturn
