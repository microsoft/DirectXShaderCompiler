RWStructuredBuffer<float> output : register(u0);

void runCase(bool choose, uint offset) {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> x =
      ::ad::bwd::variable(context, float2(1.0f, 2.0f));
  ::ad::bwd::Variable<float2> y =
      ::ad::bwd::variable(context, float2(3.0f, 4.0f));
  ::ad::bwd::Variable<float2> z =
      ::ad::bwd::variable(context, float2(5.0f, 6.0f));
  float2 primal = user::ad::bwd::f(context, x, y, z, 1, choose, 2.0f,
                                    float2(2.0f, 3.0f));
  float2 xGradient = x.gradient(context);
  float2 yGradient = y.gradient(context);
  float2 zGradient = z.gradient(context);
  output[offset] = primal.x;
  output[offset + 1] = primal.y;
  output[offset + 2] = xGradient.x;
  output[offset + 3] = xGradient.y;
  output[offset + 4] = yGradient.x;
  output[offset + 5] = yGradient.y;
  output[offset + 6] = zGradient.x;
  output[offset + 7] = zGradient.y;
}

[numthreads(1, 1, 1)]
void testMain() {
  runCase(true, 0);
  runCase(false, 8);
}
