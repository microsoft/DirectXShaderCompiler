// RUN: %dxc -E main -T cs_6_6 -Zi %s | FileCheck %s

// Check that the debug info created for a localized global variable in an
// inlined scope carries the inlinedAt chain back to the entry point, instead of
// pointing at the inlined subprogram from inside main.

// CHECK: @main

// The sampler handle is annotated at the location of one of the declares of
// the localized kSampler, all of which are inlined into main.
// CHECK: call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.+}}, %dx.types.ResourceProperties { i32 14, i32 0 }), !dbg ![[annot_loc:[0-9]+]]

// CHECK: call void @llvm.dbg.value(metadata i32 %{{.+}}, i64 0, metadata ![[helper_var:[0-9]+]], metadata !{{[0-9]+}}), !dbg ![[helper_loc:[0-9]+]] ; var:"global.g_index" !DIExpression() func:"helper"

// CHECK-DAG: ![[main_scope:[0-9]+]] = !DISubprogram(name: "main"
// CHECK-DAG: ![[helper_scope:[0-9]+]] = !DISubprogram(name: "helper"

// CHECK-DAG: ![[helper_var]] = !DILocalVariable(tag: DW_TAG_arg_variable, name: "global.g_index", arg: 0, scope: ![[helper_scope]],
// CHECK-DAG: ![[helper_loc]] = !DILocation(line: [[@LINE+6]], scope: ![[helper_scope]], inlinedAt: ![[call_loc:[0-9]+]])
// CHECK-DAG: ![[annot_loc]] = !DILocation(line: [[@LINE+4]], scope: !{{[0-9]+}}, inlinedAt: !{{[0-9]+}})
// CHECK-DAG: ![[call_loc]] = distinct !DILocation(line: {{[0-9]+}}, column: {{[0-9]+}}, scope: ![[main_scope]])

SamplerState getSampler() { return SamplerDescriptorHeap[6]; }
static const SamplerState kSampler = getSampler();
static uint g_index;

float4 helper(float uv) {
  Texture2D<float4> tex = ResourceDescriptorHeap[g_index];
  return tex.SampleLevel(kSampler, uv, 0);
}

[numthreads(1, 1, 1)] void main(uint id : SV_GroupIndex) {
  g_index = id;
  RWBuffer<float4> output = ResourceDescriptorHeap[2];
  output[0] = helper(0.5);
}
