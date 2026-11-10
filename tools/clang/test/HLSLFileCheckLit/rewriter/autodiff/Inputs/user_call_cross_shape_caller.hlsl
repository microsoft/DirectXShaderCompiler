RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> value =
      ::ad::bwd::variable(context, float2(2.0f, 3.0f));
  float4 primal = user::ad::bwd::f(
      context, value, float4(1.0f, 2.0f, 3.0f, 0.0f));
  float2 gradient = value.gradient(context);
  output[0] = primal.x;
  output[1] = primal.y;
  output[2] = primal.z;
  output[3] = gradient.x;
  output[4] = gradient.y;
}
