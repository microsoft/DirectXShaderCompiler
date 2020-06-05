// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct UBO
{
  float4x4 a;
  float4x4 b[3];
  float4 c;
};

cbuffer ubo : register(b0) { UBO ubo; }

// CHECK:   [[bName:%\d+]] = OpString "b"
// CHECK:   [[aName:%\d+]] = OpString "a"
// CHECK: [[fooName:%\d+]] = OpString "foo"

// CHECK: [[matf4v4_arr3:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugTypeArray [[dbg_f:%\d+]] %uint_3 %uint_4 %uint_4
// CHECK: OpExtInst %void [[ext]] DebugTypeMember [[bName]] [[matf4v4_arr3]]
// CHECK: [[matf4v4:%\d+]] = OpExtInst %void [[ext]] DebugTypeArray [[dbg_f]] %uint_4 %uint_4
// CHECK: OpExtInst %void [[ext]] DebugTypeMember [[aName]] [[matf4v4]]
// CHECK: OpExtInst %void [[ext]] DebugLocalVariable [[fooName]] [[matf4v4]]

void main() {
  float4x4 foo = ubo.a;
  foo += ubo.b[0] * ubo.c[0];
}
