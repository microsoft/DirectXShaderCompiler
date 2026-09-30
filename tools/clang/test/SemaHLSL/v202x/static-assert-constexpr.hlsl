// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// expected-no-diagnostics

// This test exercises a wide variety of operations using constexpr functions
// and constexpr variables inside `static_assert` expressions. Because
// `static_assert` requires its condition to be an integral constant
// expression, every case below also verifies that the compiler evaluates
// the constexpr machinery during semantic analysis (before code generation).

//===----------------------------------------------------------------------===//
// Constexpr variables in static_assert
//===----------------------------------------------------------------------===//

constexpr int kIntOne = 1;
constexpr int kIntFortyTwo = 42;
constexpr float kFloatHalf = 0.5f;
constexpr uint kUintMask = 0xF0u;

static_assert(kIntOne == 1, "literal-initialized constexpr int");
static_assert(kIntFortyTwo == 42, "literal-initialized constexpr int (42)");
static_assert(kFloatHalf == 0.5f, "literal-initialized constexpr float");
static_assert(kUintMask == 0xF0u, "literal-initialized constexpr uint");

// A constexpr variable initialized from another constexpr variable.
constexpr int kDerived = kIntFortyTwo + kIntOne;
static_assert(kDerived == 43, "constexpr initialized from constexpr");

//===----------------------------------------------------------------------===//
// Constexpr functions in static_assert
//===----------------------------------------------------------------------===//

constexpr int square(int x) { return x * x; }
constexpr int cube(int x) { return x * x * x; }
constexpr int add(int a, int b) { return a + b; }
constexpr int sub(int a, int b) { return a - b; }
constexpr int imul(int a, int b) { return a * b; }
constexpr int idiv(int a, int b) { return a / b; }
constexpr int imod(int a, int b) { return a % b; }

static_assert(square(5) == 25, "square(5)");
static_assert(cube(3) == 27, "cube(3)");
static_assert(add(3, 4) == 7, "add(3,4)");
static_assert(sub(10, 6) == 4, "sub(10,6)");
static_assert(imul(6, 7) == 42, "imul(6,7)");
static_assert(idiv(20, 4) == 5, "idiv(20,4)");
static_assert(imod(17, 5) == 2, "imod(17,5)");

//===----------------------------------------------------------------------===//
// Composition: constexpr functions with constexpr variables as arguments
//===----------------------------------------------------------------------===//

static_assert(square(kIntFortyTwo) == 1764, "square of constexpr var");
static_assert(add(kIntOne, kIntFortyTwo) == 43, "add of constexpr vars");
static_assert(imul(kIntOne, kDerived) == 43, "imul with derived constexpr");

//===----------------------------------------------------------------------===//
// Composition: constexpr functions calling constexpr functions
//===----------------------------------------------------------------------===//

constexpr int sumOfSquares(int a, int b) { return add(square(a), square(b)); }
constexpr int pyth(int a, int b) { return sumOfSquares(a, b); }

static_assert(sumOfSquares(3, 4) == 25, "3^2 + 4^2 == 25");
static_assert(pyth(5, 12) == 169, "5^2 + 12^2 == 169");
static_assert(square(square(2)) == 16, "nested square");
static_assert(add(square(2), cube(2)) == 12, "mixed nested");

//===----------------------------------------------------------------------===//
// Boolean, comparison, and logical operations
//===----------------------------------------------------------------------===//

constexpr bool lt(int a, int b) { return a < b; }
constexpr bool gt(int a, int b) { return a > b; }
constexpr bool eq(int a, int b) { return a == b; }
constexpr bool both(bool a, bool b) { return a && b; }
constexpr bool either(bool a, bool b) { return a || b; }
constexpr bool notB(bool a) { return !a; }

static_assert(lt(1, 2), "1 < 2");
static_assert(!lt(2, 1), "!(2 < 1)");
static_assert(gt(5, 4), "5 > 4");
static_assert(eq(7, 7), "7 == 7");
static_assert(both(true, true), "true && true");
static_assert(!both(true, false), "!(true && false)");
static_assert(either(false, true), "false || true");
static_assert(notB(false), "!false");

//===----------------------------------------------------------------------===//
// Ternary / conditional expressions inside constexpr
//===----------------------------------------------------------------------===//

constexpr int imax(int a, int b) { return a < b ? b : a; }
constexpr int imin(int a, int b) { return a < b ? a : b; }
constexpr int iabs(int x) { return x < 0 ? -x : x; }
constexpr int iclamp(int x, int lo, int hi) { return imax(lo, imin(x, hi)); }

static_assert(imax(3, 7) == 7, "imax(3,7)");
static_assert(imin(3, 7) == 3, "imin(3,7)");
static_assert(iabs(-9) == 9, "iabs(-9)");
static_assert(iabs(9) == 9, "iabs(9)");
static_assert(iclamp(15, 0, 10) == 10, "clamp high");
static_assert(iclamp(-5, 0, 10) == 0, "clamp low");
static_assert(iclamp(5, 0, 10) == 5, "clamp pass-through");

//===----------------------------------------------------------------------===//
// Bitwise and shift operations
//===----------------------------------------------------------------------===//

constexpr int band(int a, int b) { return a & b; }
constexpr int bor(int a, int b)  { return a | b; }
constexpr int bxor(int a, int b) { return a ^ b; }
constexpr int bnot(int a)        { return ~a; }
constexpr int shl(int a, int b)  { return a << b; }
constexpr int shr(int a, int b)  { return a >> b; }

static_assert(band(0x0F, 0x33) == 0x03, "and");
static_assert(bor(0x0F, 0x30) == 0x3F, "or");
static_assert(bxor(0xFF, 0x0F) == 0xF0, "xor");
static_assert(bnot(0) == -1, "not");
static_assert(shl(1, 4) == 16, "shl");
static_assert(shr(64, 2) == 16, "shr");
static_assert(band(kUintMask, 0x33u) == 0x30u, "bitwise with constexpr var");

//===----------------------------------------------------------------------===//
// Unary operations
//===----------------------------------------------------------------------===//

constexpr int neg(int x) { return -x; }
constexpr int pos(int x) { return +x; }

static_assert(neg(7) == -7, "neg");
static_assert(neg(neg(7)) == 7, "double neg");
static_assert(pos(7) == 7, "unary +");
static_assert(square(-3) == 9, "square of negative");

//===----------------------------------------------------------------------===//
// Floating-point constexpr
//===----------------------------------------------------------------------===//

constexpr float fadd(float a, float b) { return a + b; }
constexpr float fmul(float a, float b) { return a * b; }
constexpr float fsquare(float x) { return x * x; }

static_assert(fadd(1.5f, 2.5f) == 4.0f, "fadd");
static_assert(fmul(2.5f, 4.0f) == 10.0f, "fmul");
static_assert(fsquare(3.0f) == 9.0f, "fsquare");
static_assert(fmul(kFloatHalf, 8.0f) == 4.0f, "fmul with constexpr float var");
static_assert(fsquare(kFloatHalf) == 0.25f, "fsquare of constexpr var");

//===----------------------------------------------------------------------===//
// Casts between numeric types in constexpr
//===----------------------------------------------------------------------===//

constexpr int truncToInt(float x) { return (int)x; }
constexpr float promoteToFloat(int x) { return (float)x; }
constexpr int floatRoundTrip(int x) { return (int)((float)x * 1.0f); }

static_assert(truncToInt(3.7f) == 3, "trunc 3.7 -> 3");
static_assert(truncToInt(-3.7f) == -3, "trunc -3.7 -> -3");
static_assert(promoteToFloat(5) == 5.0f, "int -> float");
static_assert(floatRoundTrip(42) == 42, "int -> float -> int");

//===----------------------------------------------------------------------===//
// Unsigned arithmetic
//===----------------------------------------------------------------------===//

constexpr uint uadd(uint a, uint b) { return a + b; }
constexpr uint umul(uint a, uint b) { return a * b; }
constexpr uint ushift(uint a) { return a | (a >> 1); }

static_assert(uadd(3u, 4u) == 7u, "uadd");
static_assert(umul(6u, 7u) == 42u, "umul");
static_assert(ushift(4u) == 6u, "ushift");

//===----------------------------------------------------------------------===//
// Chains of constexpr variables feeding static_assert
//===----------------------------------------------------------------------===//

constexpr int kA = square(3);              // 9
constexpr int kB = add(kA, 1);             // 10
constexpr int kC = imul(kB, 2);            // 20
constexpr int kD = sumOfSquares(kA, kB);   // 81 + 100 = 181

static_assert(kA == 9, "kA");
static_assert(kB == 10, "kB");
static_assert(kC == 20, "kC");
static_assert(kD == 181, "kD");
static_assert(kA + kB + kC == 39, "sum of chain");

//===----------------------------------------------------------------------===//
// static_assert in different scopes
//===----------------------------------------------------------------------===//

namespace ns {
constexpr int nested(int x) { return x + 1; }
static_assert(nested(41) == 42, "namespace-scope static_assert");
}
static_assert(ns::nested(0) == 1, "call across namespace");

struct Holder {
  static const int kN = 4;
};
static_assert(Holder::kN == 4, "static const in struct");
static_assert(square(Holder::kN) == 16, "constexpr fn on struct constant");

//===----------------------------------------------------------------------===//
// static_assert inside a function body, including with local constexpr vars
//===----------------------------------------------------------------------===//

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  static_assert(kIntFortyTwo == 42, "global constexpr from function scope");
  static_assert(square(kIntFortyTwo) == 1764, "constexpr call from fn scope");

  constexpr int localA = add(kIntFortyTwo, 1);  // 43
  constexpr int localB = square(localA);        // 1849
  static_assert(localA == 43, "local constexpr A");
  static_assert(localB == 1849, "local constexpr B");
  static_assert(add(localA, localB) == 1892, "local constexpr composition");

  // Use the constexpr value at runtime; arr is sized by the constexpr value
  // which exercises the same constant-evaluation path as static_assert.
  int arr[localA];
  (void)arr;
}
