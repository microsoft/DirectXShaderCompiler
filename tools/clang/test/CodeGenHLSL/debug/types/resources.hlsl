// RUN: %dxc -T ps_6_0 -E main -Zi %s | FileCheck %s

// Test the definitions for resource types in debug metadata.
// Resource types should be pointer-sized and not contain members.

// CHECK-DAG: ![[empty:.*]] = !{}
// CHECK-DAG: !DICompositeType(tag: DW_TAG_class_type, name: "Texture2D<vector<float, 2> >",{{.*}} size: 32, align: 32, elements: ![[empty]]
// CHECK-DAG: !DICompositeType(tag: DW_TAG_structure_type, name: "SamplerState",{{.*}} size: 32, align: 32, elements: ![[empty]]
// CHECK-DAG: !DICompositeType(tag: DW_TAG_class_type, name: "Buffer<vector<float, 2> >",{{.*}} size: 32, align: 32, elements: ![[empty]]
// CHECK-DAG: !DICompositeType(tag: DW_TAG_structure_type, name: "ByteAddressBuffer",{{.*}} size: 32, align: 32, elements: ![[empty]]
// CHECK-DAG: !DICompositeType(tag: DW_TAG_class_type, name: "StructuredBuffer<vector<float, 2> >",{{.*}} size: 32, align: 32, elements: ![[empty]]

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

Texture2D<float2> tex;
SamplerState samp;
Buffer<float2> buf;
ByteAddressBuffer bytebuf;
StructuredBuffer<float2> structbuf;

float main() : SV_Target
{
  // Use every resource so it makes it to the debug info
  return tex.Sample(samp, 0).x + buf[0] + bytebuf.Load(0) + structbuf[0].x;
}