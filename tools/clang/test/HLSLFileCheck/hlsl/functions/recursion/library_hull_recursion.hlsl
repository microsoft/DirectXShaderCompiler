// RUN: %dxc -T lib_6_5 %s | FileCheck %s


// no error, because this function isn't exported. 
// it's reachable from main, but recurse is detected first before recurse2
void recurse2(inout float4 f, float a) {
  if (a > 0) {
    recurse2(f, a);
  }
  f -= abs(f+a);
}

// CHECK: error: recursive functions are not allowed: export function calls recursive function 'recurse'
void recurse(inout float4 f, float a)
{
    if (a > 1) {
      recurse(f, a-1);
      recurse2(f, a-1);
    }
    f += abs(f+a);
}

struct HSPerPatchData
{
  float edges[3] : SV_TessFactor;
  float inside   : SV_InsideTessFactor;
};

HSPerPatchData HSPerPatchFunc1()
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = -8;
  // CHECK: error: recursive functions are not allowed: patch constant function calls recursive function 'HSPerPatchFunc1'
  HSPerPatchFunc1();
  return d;
}

[shader("hull")]
[patchconstantfunc("HSPerPatchFunc1")]
float4 main(float a : A, float b:B) : SV_TARGET
{
  float4 f = b;
  recurse(f, a);
  return f;
}

