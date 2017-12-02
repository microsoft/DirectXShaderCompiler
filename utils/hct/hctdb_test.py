# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
###############################################################################
# This file contains driver test information for DXIL operations              #
###############################################################################

from hctdb import *
import xml.etree.ElementTree as ET
from xml.sax.saxutils import unescape

g_db_dxil = None
def get_db_dxil():
    global g_db_dxil
    if g_db_dxil is None:
        g_db_dxil = db_dxil()
    return g_db_dxil

# name to instruction pair
g_instructions = {}

# This class represents a test case for each instruction for driver testings
class test_case(object):
    def __init__(self, inst):
        self.name = ""
        self.inst = inst
        self.validation_type = ""
        self.validation_tolerance = 0
        self.inputs = []
        self.outputs = []
        self.shader_text = ""
        self.shader_target = ""
        self.test_enabled = False

"""
name: Name of the dxil operation
validation_type: validation type for test
    epsilon: absolute difference check
    ulp: units in last place check
    relative: relative error check
validation_tolerance: tolerance value for a given test
inputs: testing inputs
outputs: expected outputs for each input
shader_target: target for testing
shader_text: hlsl file that is used for testing dxil op
"""

def update_test_case(test_name, name, validation_type, validation_tolerance, input_lists, output_lists, shader_target, shader_text):
    g_instructions[name].input_lists = input_lists
    g_instructions[name].output_lists = output_lists
    g_instructions[name].validation_type = validation_type
    g_instructions[name].validation_tolerance = validation_tolerance
    g_instructions[name].shader_target = shader_target
    g_instructions[name].shader_text = shader_text
    g_instructions[name].test_enabled = True

# This is a collection of test case for driver tests per instruction
def update_test_cases():
    nan = float('nan')
    p_inf = float('inf')
    n_inf = float('-inf')
    p_denorm = float('1e-38')
    n_denorm = float('-1e-38')
    # Unary Float
    update_test_case('Sin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '-0', '-0', '0', '0', 'NaN', '-0.0007346401', '0.0007346401']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            [RootSignature("RootFlags(0), UAV(u0)")]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = sin(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Cos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '1.0', '1.0', '1.0', '1.0', 'NaN', '0.99999973015', '0.99999973015']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = cos(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Tan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '-0.0', '-0.0', '0.0', '0.0', 'NaN', '-0.000735', '0.000735']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = tan(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Hcos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', 'Inf', '1.0', '1.0', '1.0', '1.0', 'Inf', '1.543081', '1.543081']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = cosh(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Hsin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-Inf', '0.0', '0.0', '0.0', '0.0', 'Inf', '1.175201', '-1.175201']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = sinh(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Htan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-1', '-0.0', '-0.0', '0.0', '0.0', '1', '0.761594', '-0.761594']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = tanh(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Acos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5', '-1.5']],[['NaN', 'NaN', '1.570796', '1.570796', '1.570796', '1.570796', 'NaN', '0', '3.1415926', 'NaN', 'NaN']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = acos(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Asin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5', '-1.5']],[['NaN', 'NaN', '0.0', '0.0', '0.0', '0.0', 'NaN', '1.570796', '-1.570796', 'NaN', 'NaN']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = asin(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Atan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-1.570796', '0.0', '0.0', '0.0', '0.0', '1.570796', '0.785398163', '-0.785398163']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = atan(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Exp', 'Relative',21,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '10']],[['NaN', '0', '1', '1', '1', '1', 'Inf', '0.367879441', '22026.46579']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = exp(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Frc', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2.718280', '1000.599976', '-7.389']],[['NaN', 'NaN', '0', '0', '0', '0', 'NaN', '0', '0.718280', '0.599976', '0.611']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = frac(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Log', 'Relative',21,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2.718281828', '7.389056', '100']],[['NaN', 'NaN', '-Inf', '-Inf', '-Inf', '-Inf', 'Inf', 'NaN', '1.0', '1.99999998', '4.6051701']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = log(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Sqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2', '16.0', '256.0']],[['NaN', 'NaN', '-0', '-0', '0', '0', 'Inf', 'NaN', '1.41421356237', '4.0', '16.0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = sqrt(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Rsqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0', '256.0', '65536.0']],[['NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25', '0.0625', '0.00390625']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = rsqrt(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Rsqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0', '256.0', '65536.0']],[['NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25', '0.0625', '0.00390625']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = rsqrt(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Round_ne', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '11.5', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '11.0', '12.0', '-10.0', '-10.0', '-10.0', '-11.0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = round(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Round_ni', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '10.0', '-10.0', '-11.0', '-11.0', '-11.0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = floor(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Round_pi', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '11.0', '11.0', '11.0', '-10.0', '-10.0', '-10.0', '-10.0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = ceil(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('Round_z', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '10.0', '-10.0', '-10.0', '-10.0', '-10.0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = trunc(l.input);\
                g_buf[GI] = l;\
            }')
    update_test_case('IsNaN', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['1', '0', '0', '0', '0', '0', '0', '0', '0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                if (isnan(l.input))\
                    l.output = 1;\
                else\
                    l.output = 0;\
                g_buf[GI] = l;\
            }')
    update_test_case('IsInf', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['0', '1', '0', '0', '0', '0', '1', '0', '0']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                if (isinf(l.input))\
                    l.output = 1;\
                else\
                    l.output = 0;\
                g_buf[GI] = l;\
            }')
    update_test_case('IsFinite', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['0', '0', '1', '1', '1', '1', '0', '1', '1']],'cs_6_0',
    'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                if (isfinite(l.input))\
                    l.output = 1;\
                else\
                    l.output = 0;\
                g_buf[GI] = l;\
            }')
    update_test_case('FAbs', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['NaN', 'Inf', 'denorm', '0', '0', 'denorm', 'Inf', '1', '1']],'cs_6_0',
        'struct SUnaryFPOp {\
                float input;\
                float output;\
            };\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryFPOp l = g_buf[GI];\
                l.output = abs(l.input);\
                g_buf[GI] = l;\
            }')
    # Binary Float
    update_test_case('FMin', 'epsilon',0,[['-inf', '-inf', '-inf', '-inf', 'inf', 'inf', 'inf', 'inf', 'NaN', 'NaN', 'NaN', 'NaN', '1.0', '1.0', '-1.0', '-1.0', '1.0'], ['-inf', 'inf', '1.0', 'NaN', '-inf', 'inf', '1.0', 'NaN', '-inf', 'inf', '1.0', 'NaN', '-inf', 'inf', '1.0', 'NaN', '-1.0']],[['-inf', '-inf', '-inf', '-inf', '-inf', 'inf', '1.0', 'inf', '-inf', 'inf', '1.0', 'NaN', '-inf', '1.0', '-1.0', '-1.0', '-1.0'], ['-inf', 'inf', '1.0', '-inf', 'inf', 'inf', 'inf', 'inf', '-inf', 'inf', '1.0', 'NaN', '1.0', 'inf', '1.0', '-1.0', '1.0']],'cs_6_0',
        'struct SBinaryFPOp {\
                float input1;\
                float input2;\
                float output1;\
                float output2;\
            };\
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryFPOp l = g_buf[GI];\
                l.output1 = min(l.input1, l.input2);\
                l.output2 = max(l.input1, l.input2);\
                g_buf[GI] = l;\
            };')
    # Tertiary Float
    update_test_case('FMad', 'epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0', '0', '1', '1.5'], ['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0', '0', '1', '10'], ['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0', '1', '0', '-5.5']],[['NaN', 'NaN', '0', '0', '0', '0', 'Inf', '2', '0', '1', '1', '9.5']],'cs_6_0',
    'struct STertiaryFloatOp {\
                    float input1;\
                    float input2;\
                    float input3;\
                    float output;\
                };\
                RWStructuredBuffer<STertiaryFloatOp> g_buf : register(u0);\
                [numthreads(8,8,1)]\
                void main(uint GI : SV_GroupIndex) {\
                    STertiaryFloatOp l = g_buf[GI];\
                    l.output = mad(l.input1, l.input2, l.input3);\
                    g_buf[GI] = l;\
                };')
    # Unary Int
    update_test_case('Bfrev', '',0,[['-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536', '2147483647']],[['1', '65535', '536870911', '-1', '0', '-2147483648', '268435456', '32768', '-2']],'cs_6_0',
    'struct SUnaryIntOp {\
                int input;\
                int output;\
            };\
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryIntOp l = g_buf[GI];\
                l.output = reversebits(l.input);\
                g_buf[GI] = l;\
            };')
    update_test_case('FirstbitSHi', '',0,[['-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536', '2147483647']],[['30', '15', '2', '-1', '-1', '0', '3', '16', '30']],'cs_6_0',
    'struct SUnaryIntOp {\
                int input;\
                int output;\
            };\
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryIntOp l = g_buf[GI];\
                l.output = firstbithigh(l.input);\
                g_buf[GI] = l;\
            };')
    update_test_case('FirstbitLo', '',0,[['-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536', '2147483647']],[['31', '16', '3', '0', '-1', '0', '3', '16', '0']],'cs_6_0',
    'struct SUnaryIntOp {\
                int input;\
                int output;\
            };\
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryIntOp l = g_buf[GI];\
                l.output = firstbitlow(l.input);\
                g_buf[GI] = l;\
            };')
    update_test_case('Countbits', '',0,[['-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536', '2147483647']],[['1', '16', '29', '32', '0', '1', '1', '1', '31']],'cs_6_0',
    'struct SUnaryIntOp {\
                int input;\
                int output;\
            };\
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryIntOp l = g_buf[GI];\
                l.output = countbits(l.input);\
                g_buf[GI] = l;\
            };')
    update_test_case('FirstbitHi', '',0,[['0', '1', '8', '65536', '2147483647', '4294967295']],[['-1', '0', '3', '16', '30', '31']],'cs_6_0',
            'struct SUnaryUintOp {\
                uint input;\
                uint output;\
            };\
            RWStructuredBuffer<SUnaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SUnaryUintOp l = g_buf[GI];\
                l.output = firstbithigh(l.input);\
                g_buf[GI] = l;\
            };')
    # Binary Int
    update_test_case('IMax', '',0,[['-2147483648', '-10', '0', '0', '10', '2147483647'], ['0', '10', '-10', '10', '10', '0']],[['0', '10', '0', '10', '10', '2147483647']],'cs_6_0',
    'struct SBinaryIntOp {\
                int input1;\
                int input2;\
                int output1;\
                int output2;\
            };\
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryIntOp l = g_buf[GI];\
                l.output1 = max(l.input1, l.input2);\
                g_buf[GI] = l;\
            };')
    update_test_case('IMin', '',0,[['-2147483648', '-10', '0', '0', '10', '2147483647'], ['0', '10', '-10', '10', '10', '0']],[['-2147483648', '-10', '-10', '0', '10', '0']],'cs_6_0',
    'struct SBinaryIntOp {\
                int input1;\
                int input2;\
                int output1;\
                int output2;\
            };\
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryIntOp l = g_buf[GI];\
                l.output1 = min(l.input1, l.input2);\
                g_buf[GI] = l;\
            };')
    update_test_case('IMul', '',0,[['-2147483648', '-10', '-1', '0', '1', '10', '10000', '2147483647', '2147483647'], ['-10', '-10', '10', '0', '256', '4', '10001', '0', '2147483647']],[['0', '100', '-10', '0', '256', '40', '100010000', '0', '1']],'cs_6_0',
    'struct SBinaryIntOp {\
                int input1;\
                int input2;\
                int output1;\
                int output2;\
            };\
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryIntOp l = g_buf[GI];\
                l.output1 = l.input1 * l.input2;\
                g_buf[GI] = l;\
            };')
    update_test_case('UMax', '',0,[['0', '0', '10', '10000', '2147483647', '4294967295'], ['0', '256', '4', '10001', '0', '4294967295']],[['0', '256', '10', '10001', '2147483647', '4294967295']],'cs_6_0',
    'struct SBinaryUintOp {\
                uint input1;\
                uint input2;\
                uint output1;\
                uint output2;\
            };\
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryUintOp l = g_buf[GI];\
                l.output1 = max(l.input1, l.input2);\
                g_buf[GI] = l;\
            };')
    update_test_case('UMin', '',0,[['0', '0', '10', '10000', '2147483647', '4294967295'], ['0', '256', '4', '10001', '0', '4294967295']],[['0', '0', '4', '10000', '0', '4294967295']],'cs_6_0',
    'struct SBinaryUintOp {\
                uint input1;\
                uint input2;\
                uint output1;\
                uint output2;\
            };\
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryUintOp l = g_buf[GI];\
                l.output1 = min(l.input1, l.input2);\
                g_buf[GI] = l;\
            };')
    update_test_case('UMul', '',0,[['0', '1', '10', '10000', '2147483647'], ['0', '256', '4', '10001', '0']],[['0', '256', '40', '100010000', '0']],'cs_6_0',
    'struct SBinaryUintOp {\
                uint input1;\
                uint input2;\
                uint output1;\
                uint output2;\
            };\
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryUintOp l = g_buf[GI];\
                l.output1 = l.input1 * l.input2;\
                g_buf[GI] = l;\
            };')
    update_test_case('UDiv', '',0,[['1', '1', '10', '10000', '2147483647', '2147483647', '0xffffffff'], ['0', '256', '4', '10001', '0', '2147483647', '1']],[['0xffffffff', '0', '2', '0', '0xffffffff', '1', '0xffffffff'], ['0xffffffff', '1', '2', '10000', '0xffffffff', '0', '0']],'cs_6_0',
    'struct SBinaryUintOp {\
                uint input1;\
                uint input2;\
                uint output1;\
                uint output2;\
            };\
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryUintOp l = g_buf[GI];\
                l.output1 = l.input1 / l.input2;\
                l.output2 = l.input1 % l.input2;\
                g_buf[GI] = l;\
            };')
    update_test_case('UAddc', '',0,[['1', '1', '10000', '0x80000000', '0x7fffffff', '0xffffffff'], ['0', '256', '10001', '1', '0x7fffffff', '0x7fffffff']],[['2', '2', '20000', '0', '0xfffffffe', '0xfffffffe'], ['0', '512', '20002', '3', '0xfffffffe', '0xffffffff']],'cs_6_0',
    'struct SBinaryUintOp {\
                uint input1;\
                uint input2;\
                uint output1;\
                uint output2;\
            };\
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                SBinaryUintOp l = g_buf[GI];\
                uint2 x = uint2(l.input1, l.input2);\
                uint2 y = AddUint64(x, x);\
                l.output1 = y.x;\
                l.output2 = y.y;\
                g_buf[GI] = l;\
            };')

    # Tertiary Int
    update_test_case('IMad', 'epsilon',0,[['-2147483647', '-256', '-1', '0', '1', '2', '16', '2147483647', '1', '-1', '1', '10'], ['1', '-256', '-1', '0', '1', '3', '16', '0', '1', '-1', '10', '100'], ['0', '0', '0', '0', '1', '3', '1', '255', '2147483646', '-2147483647', '-10', '-2000']],[['-2147483647', '65536', '1', '0', '2', '9', '257', '255', '2147483647', '-2147483646', '0', '-1000']],'cs_6_0',
    'struct STertiaryIntOp {\
                int input1;\
                int input2;\
                int input3;\
                int output;\
            };\
            RWStructuredBuffer<STertiaryIntOp> g_buf : register(u0);\
            [numthreads(8,8,1)]\
            void main(uint GI : SV_GroupIndex) {\
                STertiaryIntOp l = g_buf[GI];\
                l.output = mad(l.input1, l.input2, l.input3);\
                g_buf[GI] = l;\
            };')

    update_test_case('UMad', 'epsilon',0,[['0', '1', '2', '16', '2147483647', '0', '10'], ['0', '1', '2', '16', '1', '0', '10'], ['0', '0', '1', '15', '0', '10', '10']],[['0', '1', '5', '271', '2147483647', '10', '110']],'cs_6_0',
    'struct STertiaryUintOp {\
                    uint input1;\
                    uint input2;\
                    uint input3;\
                    uint output;\
                };\
                RWStructuredBuffer<STertiaryUintOp> g_buf : register(u0);\
                [numthreads(8,8,1)]\
                void main(uint GI : SV_GroupIndex) {\
                    STertiaryUintOp l = g_buf[GI];\
                    l.output = mad(l.input1, l.input2, l.input3);\
                    g_buf[GI] = l;\
                };')
    
    # Dot
    update_test_case('Dot2', 'epsilon',0.008,
    [['NaN,NaN,NaN,NaN', '-Inf,-Inf,-Inf,-Inf', '-denorm,-denorm,-denorm,-denorm', '-0,-0,-0,-0', '0,0,0,0', 'denorm,denorm,denorm,denorm', 'Inf,Inf,Inf,Inf', '1,1,1,1', '-10,0,0,10', 'Inf,Inf,Inf,-Inf'], ['NaN,NaN,NaN,NaN', '-Inf,-Inf,-Inf,-Inf', '-denorm,-denorm,-denorm,-denorm', '-0,-0,-0,-0', '0,0,0,0', 'denorm,denorm,denorm,denorm', 'Inf,Inf,Inf,Inf', '1,1,1,1', '10,0,0,10', 'Inf,Inf,Inf,Inf']],
    [[nan, p_inf, 0, 0, 0, 0, p_inf, 2, -100, p_inf],
     [nan, p_inf, 0, 0, 0, 0, p_inf, 3, -100, p_inf],
     [nan, p_inf, 0, 0, 0, 0, p_inf, 4, 0, nan],
    ],
    'cs_6_0',
    'struct SDotOp {\
                   float4 input1;\
                   float4 input2;\
                   float o_dot2;\
                   float o_dot3;\
                   float o_dot4;\
                };\
                RWStructuredBuffer<SDotOp> g_buf : register(u0);\
                [numthreads(8,8,1)]\
                void main(uint GI : SV_GroupIndex) {\
                    SDotOp l = g_buf[GI];\
                    l.o_dot2 = dot(l.input1.xy, l.input2.xy);\
                    l.o_dot3 = dot(l.input1.xyz, l.input2.xyz);\
                    l.o_dot4 = dot(l.input1.xyzw, l.input2.xyzw);\
                    g_buf[GI] = l;\
                };')
    # Quaternary
    # Msad4 intrinsic calls both Bfi and Msad. Currently this is the only way to call bfi instruction from HLSL
    update_test_case('Bfi', 'epsilon',0,[
        ["0xA100B2C3", "0x00000000", "0xFFFF01C1", "0xFFFFFFFF"],
        ["0xD7B0C372, 0x4F57C2A3", "0xFFFFFFFF, 0x00000000", "0x38A03AEF, 0x38194DA3", "0xFFFFFFFF, 0x00000000"],
        ["1,2,3,4", "1,2,3,4", "0,0,0,0", "10,10,10,10"]
    ],[['153,6,92,113', '1,2,3,4', '397,585,358,707', '10,265,520,775']],'cs_6_0',
    'struct SMsad4 {\n\
                        uint ref;\n\
                        uint2 source;\n\
                        uint4 accum;\n\
                        uint4 result;\n\
                    };\n\
                    RWStructuredBuffer<SMsad4> g_buf : register(u0);\n\
                    [numthreads(8,8,1)]\n\
                    void main(uint GI : SV_GroupIndex) {\n\
                        SMsad4 l = g_buf[GI];\n\
                        l.result = msad4(l.ref, l.source, l.accum);\n\
                        g_buf[GI] = l;\n\
                    }')


# add parameter type for each table
def generate_parameter_types(table, num_inputs, num_outputs):
   param_types = ET.SubElement(table, "ParameterTypes")
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Target"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Text"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Type"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Tolerance"}).text = "double"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Warp.Version"}).text = "unsigned int" # warp version that is known to pass
   for i in range(0, num_inputs):
     ET.SubElement(param_types, "ParameterType", attrib={"Name":'Validation.Input{}'.format(i+1), 'Array':'true'}).text = "String"
   for i in range(0, num_outputs):
     ET.SubElement(param_types, "ParameterType", attrib={"Name":'Validation.Expected{}'.format(i+1), 'Array':'true'}).text = "String"

def generate_parameter_types_wave(table):
   param_types = ET.SubElement(table, "ParameterTypes")
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Target"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Text"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.NumInputSet"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.InputSet1", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.InputSet2", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.InputSet3", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.InputSet4", "Array":"true"}).text = "String"

def generate_parameter_types_msad(table):
   param_types = ET.SubElement(table, "ParameterTypes")
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Text"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Tolerance"}).text = "int"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input1", "Array":"true"}).text = "unsigned int"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input2", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input3", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Expected1", "Array":"true"}).text = "String"

def generate_row(table, case):
    row = ET.SubElement(table, "Row", {"Name":case.inst.name})
    ET.SubElement(row, "Parameter", {"Name":"Validation.Type"}).text = case.validation_type
    ET.SubElement(row, "Parameter", {"Name":"Validation.Tolerance"}).text = str(case.validation_tolerance)
    ET.SubElement(row, "Parameter", {"Name":"ShaderOp.Text"}).text = case.shader_text
    ET.SubElement(row, "Parameter", {"Name":"ShaderOp.Target"}).text = case.shader_target
    for i in range(len(case.input_lists)):
        inputs = ET.SubElement(row, "Parameter", {"Name":"Validation.Input{}".format(i+1)})
        for val in case.input_lists[i]:
            ET.SubElement(inputs, "Value").text = str(val)
    for i in range(len(case.output_lists)):
        outputs = ET.SubElement(row, "Parameter", {"Name":"Validation.Expected{}".format(i+1)})
        for val in case.output_lists[i]:
            ET.SubElement(outputs, "Value").text = str(val)

# generating xml file for execution test using data driven method
def generate_table_for_taef():
    with open("..\\..\\tools\\clang\\unittests\\HLSL\\test.xml", 'w') as f:
        tree = ET.ElementTree()
        root = ET.Element('Data')
        # Create tables
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"UnaryFloatOpTable"}), 1, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"BinaryFloatOpTable"}), 2, 2)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"TertiaryFloatOpTable"}), 3, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"UnaryIntOpTable"}), 1, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"BinaryIntOpTable"}), 2, 2)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"TertiaryIntOpTable"}), 3, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"DotOpTable"}), 2, 3)
        generate_parameter_types_msad(ET.SubElement(root, "Table", attrib={"Id":"Msad4Table"}))
        generate_parameter_types_wave(ET.SubElement(root, "Table", attrib={"Id":"WaveIntrinsicsActiveIntTable"}))
        generate_parameter_types_wave(ET.SubElement(root, "Table", attrib={"Id":"WaveIntrinsicsActiveIntTable"}))
        generate_parameter_types_wave(ET.SubElement(root, "Table", attrib={"Id":"WaveIntrinsicsActiveIntTable"}))
        generate_parameter_types_wave(ET.SubElement(root, "Table", attrib={"Id":"WaveIntrinsicsActiveIntTable"}))

        for case in [x for x in g_instructions.values() if x.test_enabled]:
            if case.inst.is_cast or case.inst.category.startswith("Unary"):
                if "f" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='UnaryFloatOpTable']"), case)
                elif "i" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='UnaryIntOpTable']"), case)
            elif case.inst.is_binary or case.inst.category.startswith("Binary"):
                if "f" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='BinaryFloatOpTable']"), case)
                elif "i" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='BinaryIntOpTable']"), case)
            elif case.inst.category.startswith("Tertiary"):
                if "f" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='TertiaryFloatOpTable']"), case)
                elif "i" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='TertiaryIntOpTable']"), case)
            elif case.inst.category.startswith("Quaternary"):
                if case.inst.name == "Bfi":
                    print("got here")
                    generate_row(root.find("./Table[@Id='Msad4Table']"), case)
            elif case.inst.category == "Dot":
                generate_row(root.find("./Table[@Id='DotOpTable']"), case)
        tree._setroot(root)
        tree.write(f)
        f.close()

def print_insts():
    for inst in g_instructions.values():
        print(inst.name, inst.is_binary, inst.category)

if __name__ == "__main__":
    db = get_db_dxil()
    for inst in db.instr:
        g_instructions[inst.name] = test_case(inst)
    update_test_cases()
    generate_table_for_taef()