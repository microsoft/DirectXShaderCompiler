#ifndef LONGVECTORTESTDATA_H
#define LONGVECTORTESTDATA_H

#include <Verify.h>
#include <limits>
#include <map>
#include <string>
#include <vector>

template <typename T> struct LongVectorTestData {
  static const std::map<std::wstring, std::vector<T>> Data;
};

template <> struct LongVectorTestData<int16_t> {
  inline static const std::map<std::wstring, std::vector<int16_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
  };
};

template <> struct LongVectorTestData<int32_t> {
  inline static const std::map<std::wstring, std::vector<int32_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
  };
};

template <> struct LongVectorTestData<int64_t> {
  inline static const std::map<std::wstring, std::vector<int64_t>> Data = {
      {L"DefaultInputValueSet1", {-6, 11, 7, 3, 8, 4, -3, 8, 8, -2}},
      {L"DefaultInputValueSet2", {5, -1337, -3, -2, 9, 3, 1, -3, 501, 2}},
  };
};

template <> struct LongVectorTestData<uint16_t> {
  inline static const std::map<std::wstring, std::vector<uint16_t>> Data = {
      {L"DefaultInputValueSet1", {1, 699, 3, 1023, 5, 6, 0, 8, 9, 10}},
      {L"DefaultInputValueSet2", {2, 111, 3, 4, 5, 9, 21, 8, 9, 10}},
  };
};

template <> struct LongVectorTestData<uint32_t> {
  inline static const std::map<std::wstring, std::vector<uint32_t>> Data = {
      {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 8, 9, 10}},
      {L"DefaultInputValueSet2", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
  };
};

template <> struct LongVectorTestData<uint64_t> {
  inline static const std::map<std::wstring, std::vector<uint64_t>> Data = {
      {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 1000, 9, 10}},
      {L"DefaultInputValueSet2", {1, 2, 1337, 4, 5, 6, 7, 8, 9, 10}},
  };
};

template <> struct LongVectorTestData<float> {
  inline static const std::map<std::wstring, std::vector<float>> Data = {
      {L"DefaultInputValueSet1",
       {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0}},
      {L"DefaultInputValueSet2",
       {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0}},
  };
};

template <> struct LongVectorTestData<double> {
  inline static const std::map<std::wstring, std::vector<double>> Data = {
      {L"DefaultInputValueSet1",
       {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0}},
      {L"DefaultInputValueSet2",
       {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0}},
  };
};

#endif // LONGVECTORTESTDATA_H
