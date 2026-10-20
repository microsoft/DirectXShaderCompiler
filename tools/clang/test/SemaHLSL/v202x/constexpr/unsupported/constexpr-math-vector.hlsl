// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr float3 absolute = abs(float3(-1.0f, 2.0f, -3.0f)); // expected-error{{constexpr variable 'absolute' must be initialized by a constant expression}}
static_assert(absolute.x == 1.0f && absolute.y == 2.0f && // expected-error{{static_assert expression is not an integral constant expression}}
                  absolute.z == 3.0f,
              "component-wise abs");

static_assert(all(bool3(true, true, true)), "all true"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(!all(bool3(true, false, true)), "not all true"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(any(bool3(false, true, false)), "any true"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(!any(bool3(false, false, false)), "none true"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(dot(int3(1, 2, 3), int3(4, 5, 6)) == 32, "integer dot"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(dot(float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f)) == // expected-error{{static_assert expression is not an integral constant expression}}
                  32.0f,
              "floating-point dot");
static_assert(length(float2(3.0f, 4.0f)) == 5.0f, "length"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(distance(float2(1.0f, 2.0f), float2(4.0f, 6.0f)) == 5.0f, // expected-error{{static_assert expression is not an integral constant expression}}
              "distance");

constexpr float3 crossed = // expected-error{{constexpr variable 'crossed' must be initialized by a constant expression}}
    cross(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f));
static_assert(crossed.x == 0.0f && crossed.y == 0.0f && crossed.z == 1.0f, // expected-error{{static_assert expression is not an integral constant expression}}
              "cross");

constexpr float3 normalized = normalize(float3(0.0f, 4.0f, 0.0f)); // expected-error{{constexpr variable 'normalized' must be initialized by a constant expression}}
static_assert(normalized.x == 0.0f && normalized.y == 1.0f && // expected-error{{static_assert expression is not an integral constant expression}}
                  normalized.z == 0.0f,
              "normalize");

constexpr float2 reflected = // expected-error{{constexpr variable 'reflected' must be initialized by a constant expression}}
    reflect(float2(1.0f, -1.0f), float2(0.0f, 1.0f));
static_assert(reflected.x == 1.0f && reflected.y == 1.0f, "reflect"); // expected-error{{static_assert expression is not an integral constant expression}}

constexpr float2 refracted = // expected-error{{constexpr variable 'refracted' must be initialized by a constant expression}}
    refract(float2(0.0f, -1.0f), float2(0.0f, 1.0f), 1.0f);
static_assert(refracted.x == 0.0f && refracted.y == -1.0f, "refract"); // expected-error{{static_assert expression is not an integral constant expression}}

constexpr float2 facing = faceforward(float2(0.0f, 1.0f), // expected-error{{constexpr variable 'facing' must be initialized by a constant expression}}
                                     float2(0.0f, 1.0f),
                                     float2(0.0f, 1.0f));
static_assert(facing.x == 0.0f && facing.y == -1.0f, "faceforward"); // expected-error{{static_assert expression is not an integral constant expression}}
