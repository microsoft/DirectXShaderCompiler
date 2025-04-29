#pragma once

#include <map>
#include <string>
#include <vector>

// A helper struct because C++ bools are 1 byte and HLSL bools are 4 bytes.
// Take int32_t as a constuctor argument and convert it to bool when needed.
// Comparisons cast to a bool because we only care if the bool representation is
// true or false.
struct HLSLBool_t {
  HLSLBool_t() : Val(0) {}
  HLSLBool_t(int32_t Val) : Val(Val) {}
  HLSLBool_t(bool Val) : Val(Val) {}
  HLSLBool_t(const HLSLBool_t &Other) : Val(Other.Val) {}

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
  HLSLHalf_t(DirectX::PackedVector::HALF Val) : Val(Val) {}
  HLSLHalf_t(const HLSLHalf_t &Other) : Val(Other.Val) {}
  HLSLHalf_t(const float F) {
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }
  HLSLHalf_t(const double D) {
    float F = 0.0f;
    if (D >= std::numeric_limits<double>::max())
      F = std::numeric_limits<float>::max();
    else if (D <= std::numeric_limits<double>::lowest())
      F = std::numeric_limits<float>::lowest();
    else
      F = static_cast<float>(D);

    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }
  HLSLHalf_t(const int I) {
    VERIFY_IS_TRUE(I == 0, L"HLSLHalf_t constructor with int override only "
                           L"meant for cases when initializing to 0.");
    const float F = static_cast<float>(I);
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }

  bool operator==(const HLSLHalf_t &Other) const { return Val == Other.Val; }

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
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    return A > F;
  }

  bool operator<(float F) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
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
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A * B));
  }

  HLSLHalf_t operator+(const HLSLHalf_t &Other) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A + B));
  }

  HLSLHalf_t operator-(const HLSLHalf_t &Other) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A - B));
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

template <typename T> struct LongVectorTestData {
  static const std::map<std::wstring, std::vector<T>> Data;
};

template <> struct LongVectorTestData<HLSLBool_t> {
  inline static const std::map<std::wstring, std::vector<HLSLBool_t>> Data = {
      {L"DefaultInputValueSet1",
       {false, true, false, false, false, false, true, true, true, true}},
      {L"DefaultInputValueSet2",
       {true, false, false, false, false, true, true, true, false, false}},
  };
};

template <> struct LongVectorTestData<int16_t> {
  inline static const std::map<std::wstring, std::vector<int16_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
      {L"DefaultClampArgs", {-1, 1}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<int32_t> {
  inline static const std::map<std::wstring, std::vector<int32_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
      {L"DefaultClampArgs", {-1, 1}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<int64_t> {
  inline static const std::map<std::wstring, std::vector<int64_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 11, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -1337, -3, -2, 9, 3, 1, -3, 501, 2}},
      {L"DefaultClampArgs", {-1, 1}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<uint16_t> {
  inline static const std::map<std::wstring, std::vector<uint16_t>> Data = {
      {L"DefaultInputValueSet1", {1, 699, 3, 1023, 5, 6, 0, 8, 9, 10}},
      {L"DefaultInputValueSet2", {2, 111, 3, 4, 5, 9, 21, 8, 9, 10}},
      {L"DefaultClampArgs", {1, 2}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<uint32_t> {
  inline static const std::map<std::wstring, std::vector<uint32_t>> Data = {
      {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 8, 9, 10}},
      {L"DefaultInputValueSet2", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
      {L"DefaultClampArgs", {1, 2}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<uint64_t> {
  inline static const std::map<std::wstring, std::vector<uint64_t>> Data = {
      {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 1000, 9, 10}},
      {L"DefaultInputValueSet2", {1, 2, 1337, 4, 5, 6, 7, 8, 9, 10}},
      {L"DefaultClampArgs", {1, 2}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<HLSLHalf_t> {
  inline static const std::map<std::wstring, std::vector<HLSLHalf_t>> Data = {
      {L"DefaultInputValueSet1",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultInputValueSet2",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultClampArgs", {-1.0, 1.0}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<float> {
  inline static const std::map<std::wstring, std::vector<float>> Data = {
      {L"DefaultInputValueSet1",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultInputValueSet2",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultClampArgs", {-1.0, 1.0}} // Min, Max values for clamp
  };
};

template <> struct LongVectorTestData<double> {
  inline static const std::map<std::wstring, std::vector<double>> Data = {
      {L"DefaultInputValueSet1",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultInputValueSet2",
       {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
      {L"DefaultClampArgs", {-1.0, 1.0}} // Min, Max values for clamp
  };
};