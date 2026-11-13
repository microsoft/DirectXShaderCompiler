RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  output[0] = original(2.0f);

  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 2.0f);
  output[1] = user::ad::bwd::f(context, x, 3.0f);
  output[2] = x.gradient(context);

  output[3] = original_custom(2.0f);
  ::ad::bwd::GradientContext<float> customContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> customX =
      ::ad::bwd::variable(customContext, 2.0f);
  output[4] = user::ad::bwd::f_custom(customContext, customX, 3.0f);
  output[5] = customX.gradient(customContext);
}
