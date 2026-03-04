#ifndef HLSLTESTDATATYPES_H
#define HLSLTESTDATATYPES_H

#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "HlslTestUtils.h"
#include "dxc/Support/Global.h"

// Shared HLSL type wrappers for use in execution tests.
// These types bridge the gap between C++ and HLSL type representations.
namespace HLSLTestDataTypes {

// A helper struct because C++ bools are 1 byte and HLSL bools are 4 bytes.
// Take int32_t as a constuctor argument and convert it to bool when needed.
// Comparisons cast to a bool because we only care if the bool representation is
// true or false.
struct HLSLBool_t {
  HLSLBool_t() : Val(0) {}
  HLSLBool_t(int32_t Val) : Val(Val) {}
  HLSLBool_t(bool Val) : Val(Val) {}

  bool operator==(const HLSLBool_t &Other) const {
    return static_cast<bool>(Val) == static_cast<bool>(Other.Val);
  }

  bool operator!=(const HLSLBool_t &Other) const {
    return static_cast<bool>(Val) != static_cast<bool>(Other.Val);
  }

  bool operator<(const HLSLBool_t &Other) const { return Val < Other.Val; }

  bool operator>(const HLSLBool_t &Other) const { return Val > Other.Val; }

  bool operator<=(const HLSLBool_t &Other) const { return Val <= Other.Val; }

  bool operator>=(const HLSLBool_t &Other) const { return Val >= Other.Val; }

  HLSLBool_t operator*(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val * Other.Val);
  }

  HLSLBool_t operator+(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val + Other.Val);
  }

  HLSLBool_t operator-(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val - Other.Val);
  }

  HLSLBool_t operator/(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val / Other.Val);
  }

  HLSLBool_t operator%(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val % Other.Val);
  }

  HLSLBool_t operator&&(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val && Other.Val);
  }

  HLSLBool_t operator||(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val || Other.Val);
  }

  bool AsBool() const { return static_cast<bool>(Val); }

  operator bool() const { return AsBool(); }
  operator int16_t() const { return (int16_t)(AsBool()); }
  operator int32_t() const { return (int32_t)(AsBool()); }
  operator int64_t() const { return (int64_t)(AsBool()); }
  operator uint16_t() const { return (uint16_t)(AsBool()); }
  operator uint32_t() const { return (uint32_t)(AsBool()); }
  operator uint64_t() const { return (uint64_t)(AsBool()); }
  operator float() const { return (float)(AsBool()); }
  operator double() const { return (double)(AsBool()); }

  // So we can construct std::wstrings using std::wostream
  friend std::wostream &operator<<(std::wostream &Os, const HLSLBool_t &Obj) {
    Os << static_cast<bool>(Obj.Val);
    return Os;
  }

  // So we can construct std::strings using std::ostream
  friend std::ostream &operator<<(std::ostream &Os, const HLSLBool_t &Obj) {
    Os << static_cast<bool>(Obj.Val);
    return Os;
  }

  int32_t Val = 0;
};

//  No native float16 type in C++ until C++23 . So we use uint16_t to represent
//  it. Simple little wrapping struct to help handle the right behavior.
struct HLSLHalf_t {
  HLSLHalf_t() : Val(0) {}
  HLSLHalf_t(const float F) {
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }
  HLSLHalf_t(const double D) {
    float F;
    if (D >= std::numeric_limits<double>::max())
      F = std::numeric_limits<float>::max();
    else if (D <= std::numeric_limits<double>::lowest())
      F = std::numeric_limits<float>::lowest();
    else
      F = static_cast<float>(D);

    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }
  HLSLHalf_t(const uint32_t U) {
    float F = static_cast<float>(U);
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }

  // PackedVector::HALF is a uint16. Make sure we don't ever accidentally
  // convert one of these to a HLSLHalf_t by arithmetically converting it to a
  // float.
  HLSLHalf_t(DirectX::PackedVector::HALF) = delete;

  static double GetULP(HLSLHalf_t A) {
    DXASSERT(!std::isnan(A) && !std::isinf(A),
             "ULP of NaN or infinity is undefined");

    HLSLHalf_t Next = A;
    ++Next.Val;

    double NextD = Next;
    double AD = A;
    return NextD - AD;
  }

  static HLSLHalf_t FromHALF(DirectX::PackedVector::HALF Half) {
    HLSLHalf_t H;
    H.Val = Half;
    return H;
  }

  // Implicit conversion to float for use with things like std::acos, std::tan,
  // etc
  operator float() const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val);
  }

  bool operator==(const HLSLHalf_t &Other) const {
    // Convert to floats to properly handle the '0 == -0' case which must
    // compare to true but have different uint16_t values.
    // That is, 0 == -0 is true. We store Val as a uint16_t.
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return A == B;
  }

  bool operator<(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) <
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator>(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) >
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  // Used by tolerance checks in the tests.
  bool operator>(float F) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    return A > F;
  }

  bool operator<(float F) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    return A < F;
  }

  bool operator<=(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) <=
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator>=(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) >=
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator!=(const HLSLHalf_t &Other) const { return Val != Other.Val; }

  HLSLHalf_t operator*(const HLSLHalf_t &Other) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return FromHALF(DirectX::PackedVector::XMConvertFloatToHalf(A * B));
  }

  HLSLHalf_t operator+(const HLSLHalf_t &Other) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return FromHALF((DirectX::PackedVector::XMConvertFloatToHalf(A + B)));
  }

  HLSLHalf_t operator-(const HLSLHalf_t &Other) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return FromHALF(DirectX::PackedVector::XMConvertFloatToHalf(A - B));
  }

  HLSLHalf_t operator/(const HLSLHalf_t &Other) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return FromHALF(DirectX::PackedVector::XMConvertFloatToHalf(A / B));
  }

  HLSLHalf_t operator%(const HLSLHalf_t &Other) const {
    const float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    const float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    const float C = std::fmod(A, B);
    return FromHALF(DirectX::PackedVector::XMConvertFloatToHalf(C));
  }

  // So we can construct std::wstrings using std::wostream
  friend std::wostream &operator<<(std::wostream &Os, const HLSLHalf_t &Obj) {
    Os << DirectX::PackedVector::XMConvertHalfToFloat(Obj.Val);
    return Os;
  }

  // So we can construct std::wstrings using std::wostream
  friend std::ostream &operator<<(std::ostream &Os, const HLSLHalf_t &Obj) {
    Os << DirectX::PackedVector::XMConvertHalfToFloat(Obj.Val);
    return Os;
  }

  // HALF is an alias to uint16_t
  DirectX::PackedVector::HALF Val = 0;
};

// Normalized type wrappers for SNorm [-1,1] and UNorm [0,1] interpretations.
// Thin wrappers over floating-point types to enable type-distinct input sets.
// The Signed parameter distinguishes SNorm (true) from UNorm (false).
template <typename BaseT, bool Signed> struct HLSLNorm_t {
  BaseT Val;

  HLSLNorm_t() : Val() {}
  HLSLNorm_t(BaseT V) : Val(V) {}

  operator BaseT() const { return Val; }

  HLSLNorm_t operator*(const HLSLNorm_t &O) const {
    return HLSLNorm_t(Val * O.Val);
  }
  HLSLNorm_t operator+(const HLSLNorm_t &O) const {
    return HLSLNorm_t(Val + O.Val);
  }
  HLSLNorm_t operator-(const HLSLNorm_t &O) const {
    return HLSLNorm_t(Val - O.Val);
  }

  bool operator==(const HLSLNorm_t &O) const { return Val == O.Val; }
  bool operator!=(const HLSLNorm_t &O) const { return !(Val == O.Val); }
  bool operator<(const HLSLNorm_t &O) const { return Val < O.Val; }
  bool operator>(const HLSLNorm_t &O) const { return Val > O.Val; }
  bool operator<=(const HLSLNorm_t &O) const { return Val <= O.Val; }
  bool operator>=(const HLSLNorm_t &O) const { return Val >= O.Val; }

  friend std::ostream &operator<<(std::ostream &Os, const HLSLNorm_t &Obj) {
    Os << Obj.Val;
    return Os;
  }
  friend std::wostream &operator<<(std::wostream &Os, const HLSLNorm_t &Obj) {
    Os << Obj.Val;
    return Os;
  }
};

using SNormF16_t = HLSLNorm_t<HLSLHalf_t, true>;
using UNormF16_t = HLSLNorm_t<HLSLHalf_t, false>;
using SNormF32_t = HLSLNorm_t<float, true>;
using UNormF32_t = HLSLNorm_t<float, false>;
using SNormF64_t = HLSLNorm_t<double, true>;
using UNormF64_t = HLSLNorm_t<double, false>;

// FP8 E4M3 type wrapper (1 sign, 4 exponent, 3 mantissa, bias 7).
// Range: [-448, 448]. No Inf; only NaN (0x7F/0xFF).
struct F8E4M3_t {
  uint8_t Val;

  F8E4M3_t() : Val(0) {}
  F8E4M3_t(float F) { Val = FloatToF8E4M3(F); }

  operator float() const { return F8E4M3ToFloat(Val); }

  F8E4M3_t operator*(const F8E4M3_t &O) const {
    return F8E4M3_t(float(*this) * float(O));
  }
  F8E4M3_t operator+(const F8E4M3_t &O) const {
    return F8E4M3_t(float(*this) + float(O));
  }
  F8E4M3_t operator-(const F8E4M3_t &O) const {
    return F8E4M3_t(float(*this) - float(O));
  }

  bool operator==(const F8E4M3_t &O) const { return Val == O.Val; }
  bool operator!=(const F8E4M3_t &O) const { return Val != O.Val; }
  bool operator<(const F8E4M3_t &O) const { return float(*this) < float(O); }
  bool operator>(const F8E4M3_t &O) const { return float(*this) > float(O); }
  bool operator<=(const F8E4M3_t &O) const { return float(*this) <= float(O); }
  bool operator>=(const F8E4M3_t &O) const { return float(*this) >= float(O); }

  friend std::ostream &operator<<(std::ostream &Os, const F8E4M3_t &Obj) {
    Os << float(Obj);
    return Os;
  }
  friend std::wostream &operator<<(std::wostream &Os, const F8E4M3_t &Obj) {
    Os << float(Obj);
    return Os;
  }

private:
  static float F8E4M3ToFloat(uint8_t V) {
    uint8_t Sign = (V >> 7) & 1;
    uint8_t Exp = (V >> 3) & 0xF;
    uint8_t Mant = V & 0x7;

    if (Exp == 0xF && Mant == 0x7)
      return std::numeric_limits<float>::quiet_NaN();

    float Result;
    if (Exp == 0)
      Result = std::ldexp(static_cast<float>(Mant), -9);
    else
      Result = std::ldexp(1.0f + static_cast<float>(Mant) / 8.0f, Exp - 7);

    return Sign ? -Result : Result;
  }

  static uint8_t FloatToF8E4M3(float F) {
    if (std::isnan(F))
      return 0x7F;

    uint8_t Sign = 0;
    if (F < 0.0f) {
      Sign = 1;
      F = -F;
    }

    if (F == 0.0f)
      return Sign << 7;

    // Clamp to max representable (E=15, M=6 → 448).
    if (F >= 448.0f)
      return (Sign << 7) | (0xF << 3) | 0x6;

    int Exp;
    float Frac = std::frexp(F, &Exp);
    int BiasedExp = Exp + 6;

    if (BiasedExp <= 0) {
      int Mant = static_cast<int>(std::round(F * 512.0f));
      if (Mant > 7)
        Mant = 7;
      if (Mant < 0)
        Mant = 0;
      return (Sign << 7) | static_cast<uint8_t>(Mant);
    }

    float Significand = 2.0f * Frac;
    int Mant = static_cast<int>(std::round((Significand - 1.0f) * 8.0f));

    if (Mant >= 8) {
      Mant = 0;
      BiasedExp++;
    }

    if (BiasedExp >= 15) {
      if (BiasedExp > 15 || Mant > 6)
        return (Sign << 7) | (0xF << 3) | 0x6;
    }

    return (Sign << 7) | (static_cast<uint8_t>(BiasedExp) << 3) |
           static_cast<uint8_t>(Mant);
  }
};

// FP8 E5M2 type wrapper (1 sign, 5 exponent, 2 mantissa, bias 15).
// Range: [-57344, 57344]. Has Inf and NaN (like IEEE 754).
struct F8E5M2_t {
  uint8_t Val;

  F8E5M2_t() : Val(0) {}
  F8E5M2_t(float F) { Val = FloatToF8E5M2(F); }

  operator float() const { return F8E5M2ToFloat(Val); }

  F8E5M2_t operator*(const F8E5M2_t &O) const {
    return F8E5M2_t(float(*this) * float(O));
  }
  F8E5M2_t operator+(const F8E5M2_t &O) const {
    return F8E5M2_t(float(*this) + float(O));
  }
  F8E5M2_t operator-(const F8E5M2_t &O) const {
    return F8E5M2_t(float(*this) - float(O));
  }

  bool operator==(const F8E5M2_t &O) const { return Val == O.Val; }
  bool operator!=(const F8E5M2_t &O) const { return Val != O.Val; }
  bool operator<(const F8E5M2_t &O) const { return float(*this) < float(O); }
  bool operator>(const F8E5M2_t &O) const { return float(*this) > float(O); }
  bool operator<=(const F8E5M2_t &O) const { return float(*this) <= float(O); }
  bool operator>=(const F8E5M2_t &O) const { return float(*this) >= float(O); }

  friend std::ostream &operator<<(std::ostream &Os, const F8E5M2_t &Obj) {
    Os << float(Obj);
    return Os;
  }
  friend std::wostream &operator<<(std::wostream &Os, const F8E5M2_t &Obj) {
    Os << float(Obj);
    return Os;
  }

private:
  static float F8E5M2ToFloat(uint8_t V) {
    uint8_t Sign = (V >> 7) & 1;
    uint8_t Exp = (V >> 2) & 0x1F;
    uint8_t Mant = V & 0x3;

    if (Exp == 0x1F) {
      if (Mant == 0)
        return Sign ? -std::numeric_limits<float>::infinity()
                    : std::numeric_limits<float>::infinity();
      return std::numeric_limits<float>::quiet_NaN();
    }

    float Result;
    if (Exp == 0)
      Result = std::ldexp(static_cast<float>(Mant), -16);
    else
      Result = std::ldexp(1.0f + static_cast<float>(Mant) / 4.0f, Exp - 15);

    return Sign ? -Result : Result;
  }

  static uint8_t FloatToF8E5M2(float F) {
    if (std::isnan(F))
      return 0x7F;

    uint8_t Sign = 0;
    if (F < 0.0f) {
      Sign = 1;
      F = -F;
    }

    if (std::isinf(F))
      return (Sign << 7) | (0x1F << 2);

    if (F == 0.0f)
      return Sign << 7;

    // Clamp to max representable (E=30, M=3 → 57344).
    if (F >= 57344.0f)
      return (Sign << 7) | (0x1E << 2) | 0x3;

    int Exp;
    float Frac = std::frexp(F, &Exp);
    int BiasedExp = Exp + 14;

    if (BiasedExp <= 0) {
      int Mant = static_cast<int>(std::round(F * 65536.0f));
      if (Mant > 3)
        Mant = 3;
      if (Mant < 0)
        Mant = 0;
      return (Sign << 7) | static_cast<uint8_t>(Mant);
    }

    float Significand = 2.0f * Frac;
    int Mant = static_cast<int>(std::round((Significand - 1.0f) * 4.0f));

    if (Mant >= 4) {
      Mant = 0;
      BiasedExp++;
    }

    if (BiasedExp >= 31)
      return (Sign << 7) | (0x1F << 2);

    return (Sign << 7) | (static_cast<uint8_t>(BiasedExp) << 2) |
           static_cast<uint8_t>(Mant);
  }
};

//
// Shared type traits and validation infrastructure.
//

template <typename T> constexpr bool isFloatingPointType() {
  return std::is_same_v<T, float> || std::is_same_v<T, double> ||
         std::is_same_v<T, HLSLHalf_t>;
}

enum class ValidationType {
  Epsilon,
  Ulp,
};

struct ValidationConfig {
  double Tolerance = 0.0;
  ValidationType Type = ValidationType::Epsilon;

  static ValidationConfig Epsilon(double Tolerance) {
    return ValidationConfig{Tolerance, ValidationType::Epsilon};
  }

  static ValidationConfig Ulp(double Tolerance) {
    return ValidationConfig{Tolerance, ValidationType::Ulp};
  }
};

// Default validation: ULP for floating point, exact for integers.
template <typename T> struct DefaultValidation {
  ValidationConfig ValidationConfig;

  DefaultValidation() {
    if constexpr (isFloatingPointType<T>())
      ValidationConfig = ValidationConfig::Ulp(1.0f);
  }
};

// Strict validation: exact match by default.
struct StrictValidation {
  ValidationConfig ValidationConfig;
};

//
// Value comparison overloads used by both LongVector and LinearAlgebra tests.
//

template <typename T>
inline bool doValuesMatch(T A, T B, double Tolerance, ValidationType) {
  if (Tolerance == 0.0)
    return A == B;

  T Diff = A > B ? A - B : B - A;
  return Diff <= Tolerance;
}

inline bool doValuesMatch(HLSLBool_t A, HLSLBool_t B, double, ValidationType) {
  return A == B;
}

inline bool doValuesMatch(HLSLHalf_t A, HLSLHalf_t B, double Tolerance,
                          ValidationType VType) {
  switch (VType) {
  case ValidationType::Epsilon:
    return CompareHalfEpsilon(A.Val, B.Val, static_cast<float>(Tolerance));
  case ValidationType::Ulp:
    return CompareHalfULP(A.Val, B.Val, static_cast<float>(Tolerance));
  default:
    hlsl_test::LogErrorFmt(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

inline bool doValuesMatch(float A, float B, double Tolerance,
                          ValidationType VType) {
  switch (VType) {
  case ValidationType::Epsilon:
    return CompareFloatEpsilon(A, B, static_cast<float>(Tolerance));
  case ValidationType::Ulp: {
    const int IntTolerance = static_cast<int>(Tolerance);
    return CompareFloatULP(A, B, IntTolerance);
  }
  default:
    hlsl_test::LogErrorFmt(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

inline bool doValuesMatch(double A, double B, double Tolerance,
                          ValidationType VType) {
  switch (VType) {
  case ValidationType::Epsilon:
    return CompareDoubleEpsilon(A, B, Tolerance);
  case ValidationType::Ulp: {
    const int64_t IntTolerance = static_cast<int64_t>(Tolerance);
    return CompareDoubleULP(A, B, IntTolerance);
  }
  default:
    hlsl_test::LogErrorFmt(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

} // namespace HLSLTestDataTypes

#endif // HLSLTESTDATATYPES_H
