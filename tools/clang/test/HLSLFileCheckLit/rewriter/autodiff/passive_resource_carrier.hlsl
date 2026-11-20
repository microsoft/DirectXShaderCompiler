// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/passive_resource_carrier_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: float invoke_fetch(inout GradientContext<float> context, Texture2D<float> texture, uint index, float __dxc_ad_seed)
// CHECK: ::bwd_fetch(texture, index, __dxc_ad_seed);
// CHECK: float direct_resources(inout GradientContext<float> context, Texture2D<float> texture, SamplerState sampler, RWStructuredBuffer<int> sink, Variable<float> value, float __dxc_ad_seed)
// CHECK: float nested_resource(inout GradientContext<float> context, NestedCarrier carrier, Variable<float> value, float __dxc_ad_seed)
// CHECK: float global_resource(inout GradientContext<float> context, Variable<float> value, float __dxc_ad_seed)
// CHECK: float evaluate(inout GradientContext<float> context, ResourceCarrier carrier, Variable<float> value, uint index, float __dxc_ad_seed)
// CHECK-NOT: GradientContext<ResourceCarrier>
// CHECK-NOT: Variable<ResourceCarrier>
// CHECK: carrier.bwd_read(index, (__dxc_ad_seed * value.value));

// EXEC: atomicBinOp.i32
// EXEC: atomicBinOp.i32
// EXEC: atomicBinOp.i32
// EXEC: atomicBinOp.i32
// EXEC: rawBufferStore.f32

Texture2D<float> source : register(t0);
SamplerState sourceSampler : register(s0);
RWStructuredBuffer<int> gradientSink : register(u0);

struct ResourceCarrier {
  Texture2D<float> texture;
  RWStructuredBuffer<int> sink;

  [[dxc::BackwardDerivative(bwd_read)]]
  float read([[dxc::NoDiff]] uint index) {
    return texture.Load(int3(index, 0, 0));
  }

  void bwd_read([[dxc::NoDiff]] uint index, float dResult) {
    int fixedPoint = (int)(dResult * 65536.0f);
    InterlockedAdd(sink[index * 4], fixedPoint);
    InterlockedAdd(sink[index * 4 + 1], fixedPoint);
    InterlockedAdd(sink[index * 4 + 2], fixedPoint);
    InterlockedAdd(sink[index * 4 + 3], 1);
  }
};

struct NestedCarrier {
  ResourceCarrier resources;
  float scale;
};

void bwd_fetch(Texture2D<float> texture, [[dxc::NoDiff]] uint index,
               float dResult) {
  InterlockedAdd(gradientSink[index], (int)(dResult * 65536.0f));
}

[[dxc::BackwardDerivative(bwd_fetch)]]
float fetch(Texture2D<float> texture, [[dxc::NoDiff]] uint index) {
  return texture.Load(int3(index, 0, 0));
}

[[dxc::AutoDiff(bwd)]]
float invoke_fetch(Texture2D<float> texture,
                   [[dxc::NoDiff]] uint index) {
  return fetch(texture, index);
}

[[dxc::AutoDiff(bwd)]]
float direct_resources(Texture2D<float> texture, SamplerState sampler,
                       RWStructuredBuffer<int> sink, float value) {
  return value;
}

[[dxc::AutoDiff(bwd)]]
float nested_resource(NestedCarrier carrier, float value) {
  return carrier.scale * value;
}

[[dxc::AutoDiff(bwd)]]
float global_resource(float value) { return source.Load(int3(0, 0, 0)) * value; }

[[dxc::AutoDiff(bwd)]]
float evaluate(ResourceCarrier carrier, float value,
               [[dxc::NoDiff]] uint index) {
  return carrier.read(index) * value;
}
