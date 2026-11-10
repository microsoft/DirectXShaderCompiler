RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 1.0f);
  ::ad::bwd::Variable<float> y = ::ad::bwd::variable(context, 5.0f);
  output[0] = user::ad::bwd::f(context, x, y, 2, 2.0f, 2.0f);
  output[1] = x.gradient(context);
  output[2] = y.gradient(context);
}
