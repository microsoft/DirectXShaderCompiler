// RUN: %dxc -T vs_6_9 %s -verify

// expected-note@+1{{entry function defined here}}
float main(RayDesc rayDesc: RAYDESC) : OUT {
// expected-error@+1{{Shader kind 'vertex' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  HitObject hit;
  return 0.f;
}
