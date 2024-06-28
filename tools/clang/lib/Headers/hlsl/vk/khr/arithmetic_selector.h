// Copyright (c) 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _HLSL_VK_KHR_ARITHMETIC_SELECTOR_H_
#define _HLSL_VK_KHR_ARITHMETIC_SELECTOR_H_

#define DECLARE_UNARY_OP(name, opcode)                                         \
  template <typename ResultType>                                               \
  [[vk::ext_instruction(opcode)]] ResultType __builtin_spv_##name(             \
      ResultType a)

DECLARE_UNARY_OP(SNegate, 126);
DECLARE_UNARY_OP(FNegate, 127);

#undef DECLARY_UNARY_OP

#define DECLARE_BINOP(name, opcode)                                            \
  template <typename ResultType>                                               \
  [[vk::ext_instruction(opcode)]] ResultType __builtin_spv_##name(             \
      ResultType a, ResultType b)

DECLARE_BINOP(IAdd, 128);
DECLARE_BINOP(FAdd, 129);
DECLARE_BINOP(ISub, 130);
DECLARE_BINOP(FSub, 131);
DECLARE_BINOP(IMul, 132);
DECLARE_BINOP(FMul, 133);
DECLARE_BINOP(UDiv, 134);
DECLARE_BINOP(SDiv, 135);
DECLARE_BINOP(FDiv, 136);

#undef DECLARE_BINOP
namespace vk {
namespace util {

template <class ComponentType> class ArithmeticSelector;

#define ARITHMETIC_SELECTOR(BaseType, OpNegate, OpAdd, OpSub, OpDiv,           \
                            SIGNED_INTEGER_TYPE)                               \
  template <> class ArithmeticSelector<BaseType> {                             \
    template <class T> static T Negate(T a) { return OpNegate(a); }            \
    template <class T> static T Add(T a, T b) { return OpAdd(a, b); }          \
    template <class T> static T Sub(T a, T b) { return OpSub(a, b); }          \
    template <class T> static T Mul(T a, T b) { return OpMul(a, b); }          \
    template <class T> static T Div(T a, T b) { return OpDiv(a, b); }          \
  };

ARITHMETIC_SELECTOR(half, __builtin_spv_FNegate, __builtin_spv_FAdd,
                    __builtin_spv_FSub, __builtin_spv_FDiv, false);
ARITHMETIC_SELECTOR(float, __builtin_spv_FNegate, __builtin_spv_FAdd,
                    __builtin_spv_FSub, __builtin_spv_FDiv, false);
ARITHMETIC_SELECTOR(double, __builtin_spv_FNegate, __builtin_spv_FAdd,
                    __builtin_spv_FSub, __builtin_spv_FDiv, false);

#if __HLSL_ENABLE_16_BIT
ARITHMETIC_SELECTOR(int16_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_SDiv, true);
ARITHMETIC_SELECTOR(uint16_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_UDiv, false);
#endif // __HLSL_ENABLE_16_BIT

ARITHMETIC_SELECTOR(int32_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_SDiv, true);
ARITHMETIC_SELECTOR(int64_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_SDiv, true);
ARITHMETIC_SELECTOR(uint32_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_SDiv, false);
ARITHMETIC_SELECTOR(uint64_t, __builtin_spv_SNegate, __builtin_spv_IAdd,
                    __builtin_spv_ISub, __builtin_spv_SDiv, false);
} // namespace util
} // namespace vk

#endif // _HLSL_VK_KHR_ARITHMETIC_SELECTOR_H_