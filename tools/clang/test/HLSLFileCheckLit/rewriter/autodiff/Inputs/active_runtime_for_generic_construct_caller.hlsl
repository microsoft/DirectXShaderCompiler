RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> x =
      ::ad::bwd::variable(context, float2(1.0f, 2.0f));
  ::ad::bwd::Variable<float2> y =
      ::ad::bwd::variable(context, float2(3.0f, 4.0f));
  ::ad::bwd::Variable<float2> z =
      ::ad::bwd::variable(context, float2(5.0f, 6.0f));
  float2 primal =
      user::ad::bwd::f(context, x, y, z, 1, 2.0f, float2(2.0f, 3.0f));
  float2 xGradient = x.gradient(context);
  float2 yGradient = y.gradient(context);
  float2 zGradient = z.gradient(context);
  output[0] = primal.x;
  output[1] = primal.y;
  output[2] = xGradient.x;
  output[3] = xGradient.y;
  output[4] = yGradient.x;
  output[5] = yGradient.y;
  output[6] = zGradient.x;
  output[7] = zGradient.y;
}