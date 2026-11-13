RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> uv =
      ::ad::bwd::variable(context, float2(2.0f, 5.0f));
  float primal = user::ad::bwd::f(context, uv, 1u, 3.0f);
  float2 gradient = uv.gradient(context);
  output[0] = primal;
  output[1] = gradient.x;
  output[2] = gradient.y;
}
