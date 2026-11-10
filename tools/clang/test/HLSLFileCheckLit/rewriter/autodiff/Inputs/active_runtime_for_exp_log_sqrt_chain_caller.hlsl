RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> expContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> expX = ::ad::bwd::variable(expContext, 0.0f);
  ::ad::bwd::Variable<float> expY = ::ad::bwd::variable(expContext, 2.0f);
  ::ad::bwd::Variable<float> expZ = ::ad::bwd::variable(expContext, 3.0f);
  output[0] = user::ad::bwd::expRecurrence(expContext, expX, expY, expZ, 1,
                                            2.0f, 2.0f);
  output[1] = expX.gradient(expContext);
  output[2] = expY.gradient(expContext);
  output[3] = expZ.gradient(expContext);

  ::ad::bwd::GradientContext<float> rootContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> rootX = ::ad::bwd::variable(rootContext, 1.0f);
  ::ad::bwd::Variable<float> rootY = ::ad::bwd::variable(rootContext, 2.0f);
  ::ad::bwd::Variable<float> rootZ = ::ad::bwd::variable(rootContext, 3.0f);
  output[4] = user::ad::bwd::logSqrtRecurrence(
      rootContext, rootX, rootY, rootZ, 1, 2.0f, 2.0f);
  output[5] = rootX.gradient(rootContext);
  output[6] = rootY.gradient(rootContext);
  output[7] = rootZ.gradient(rootContext);
}
