RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> uv =
      ::ad::bwd::variable(context, float2(2.0f, 3.0f));
  float4 primal = user::ad::bwd::f(
      context, uv, float4(4.0f, 5.0f, 6.0f, 7.0f));
  float2 gradient = uv.gradient(context);
  output[0] = primal.x;
  output[1] = primal.y;
  output[2] = primal.z;
  output[3] = primal.w;
  output[4] = gradient.x;
  output[5] = gradient.y;
}
