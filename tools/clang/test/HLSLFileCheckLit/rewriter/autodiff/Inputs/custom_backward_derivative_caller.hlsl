RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 2.0f);
  output[0] = user::ad::bwd::f(context, x, 3.0f);
  output[1] = x.gradient(context);

  ::ad::bwd::GradientContext<float> loopContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> loopX =
      ::ad::bwd::variable(loopContext, 2.0f);
  output[2] = user::ad::bwd::loop(loopContext, loopX, 3, 3.0f);
  output[3] = loopX.gradient(loopContext);

  ::ad::bwd::GradientContext<float2> vectorContext =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> vectorX =
      ::ad::bwd::variable(vectorContext, float2(2.0f, 3.0f));
  float2 vectorResult = user::ad::bwd::vector_f(
      vectorContext, vectorX, float2(5.0f, 7.0f));
  float2 vectorGradient = vectorX.gradient(vectorContext);
  output[4] = vectorResult.x;
  output[5] = vectorResult.y;
  output[6] = vectorGradient.x;
  output[7] = vectorGradient.y;

  ::ad::bwd::GradientContext<float> lateContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> lateX =
      ::ad::bwd::variable(lateContext, 2.0f);
  output[8] = user::ad::bwd::late_f(lateContext, lateX, 3.0f);
  output[9] = lateX.gradient(lateContext);

  ::ad::bwd::GradientContext<float> overloadedContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> overloadedX =
      ::ad::bwd::variable(overloadedContext, 2.0f);
  output[10] =
      user::ad::bwd::overloaded_f(overloadedContext, overloadedX, 3.0f);
  output[11] = overloadedX.gradient(overloadedContext);

  ::ad::bwd::GradientContext<float> pairContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> pairX =
      ::ad::bwd::variable(pairContext, 2.0f);
  ::ad::bwd::Variable<float> pairY =
      ::ad::bwd::variable(pairContext, 3.0f);
  output[12] =
      user::ad::bwd::pair_f(pairContext, pairX, pairY, 3.0f);
  output[13] = pairX.gradient(pairContext);
  output[14] = pairY.gradient(pairContext);
}