RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float3> context =
      (::ad::bwd::GradientContext<float3>)0;
  ::ad::bwd::Variable<float3> value =
      ::ad::bwd::variable(context, float3(1.0f, 2.0f, 3.0f));
  float3 primal = user::ad::bwd::f(
      context, float2(10.0f, 20.0f), float4(30.0f, 40.0f, 50.0f, 60.0f),
      value, float3(4.0f, 5.0f, 6.0f));
  float3 gradient = value.gradient(context);
  output[0] = primal.x;
  output[1] = primal.y;
  output[2] = primal.z;
  output[3] = gradient.x;
  output[4] = gradient.y;
  output[5] = gradient.z;
}
