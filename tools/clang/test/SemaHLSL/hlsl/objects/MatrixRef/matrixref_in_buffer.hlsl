// REQUIRES dxil-1-10
// RUN: %dxc -T lib_6_10 %s -verify

// expected-error@+1{{object '__builtin_la_MatrixRef' is not allowed in builtin template parameters}}
RWStructuredBuffer<__builtin_la_MatrixRef> InvalidBuffer;
