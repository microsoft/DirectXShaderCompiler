#pragma once

#include <map>
#include <vector>
#include <string>

static const std::map<std::wstring, std::vector<bool>> DefaultInputValueSet_bool = {
    {L"DefaultInputValueSet1", {false, true, false, false, false, false, true, true, true, true}},
    {L"DefaultInputValueSet2", {true, false, false, false, false, true, true, true, false, false}},
};

static const std::map<std::wstring, std::vector<int16_t>> DefaultInputValueSet_int16 = {
    {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
    {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
    {L"DefaultClampArgs", {-1 , 1}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<int32_t>> DefaultInputValueSet_int32 = {
    {L"DefaultInputValueSet1", {-6, 1, 7, 3, 8, 4, -3, 8, 8, -2}},
    {L"DefaultInputValueSet2", {5, -6, -3, -2, 9, 3, 1, -3, -7, 2}},
    {L"DefaultClampArgs", {-1 , 1}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<int64_t>> DefaultInputValueSet_int64 = {
    {L"DefaultInputValueSet1", {-6, 11, 7, 3, 8, 4, -3, 8, 8, -2}},
    {L"DefaultInputValueSet2", {5, -1337, -3, -2, 9, 3, 1, -3, 501, 2}},
    {L"DefaultClampArgs", {-1 , 1}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<uint16_t>> DefaultInputValueSet_uint16 = {
    {L"DefaultInputValueSet1", {1, 699, 3, 1023, 5, 6, 0, 8, 9, 10}},
    {L"DefaultInputValueSet2", {2, 111, 3, 4, 5, 9, 21, 8, 9, 10}},
    {L"DefaultClampArgs", {1 , 2}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<uint32_t>> DefaultInputValueSet_uint32 = {
    {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 8, 9, 10}},
    {L"DefaultInputValueSet2", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
    {L"DefaultClampArgs", {1 , 2}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<uint64_t>> DefaultInputValueSet_uint64 = {
    {L"DefaultInputValueSet1", {1, 2, 3, 4, 5, 0, 7, 1000, 9, 10}},
    {L"DefaultInputValueSet2", {1, 2, 1337, 4, 5, 6, 7, 8, 9, 10}},
    {L"DefaultClampArgs", {1 , 2}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<float>> DefaultInputValueSet_float16 = {
    {L"DefaultInputValueSet1", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultInputValueSet2", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultClampArgs", {-1.0 , 1.0}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<float>> DefaultInputValueSet_float32 = {
    {L"DefaultInputValueSet1", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultInputValueSet2", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultClampArgs", {-1.0 , 1.0}} // Min, Max values for clamp
};

static const std::map<std::wstring, std::vector<double>> DefaultInputValueSet_float64 = {
    {L"DefaultInputValueSet1", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultInputValueSet2", {1.0, -2.0, 3.0, -4.0, 5.0, -6.0, 7.0, -8.0, 9.0, -10.0}},
    {L"DefaultClampArgs", {-1.0 , 1.0}} // Min, Max values for clamp
};