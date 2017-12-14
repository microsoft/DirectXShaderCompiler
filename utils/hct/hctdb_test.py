# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
###############################################################################
# This file contains driver test information for DXIL operations              #
###############################################################################

from hctdb import *
import xml.etree.ElementTree as ET
import argparse

parser = argparse.ArgumentParser(description="contains information about dxil op test cases.")
parser.add_argument('mode', help="'gen-xml' or 'info'")

g_db_dxil = None

def get_db_dxil():
    global g_db_dxil
    if g_db_dxil is None:
        g_db_dxil = db_dxil()
    return g_db_dxil

"""
This class represents a test case for instructions for driver testings

DXIL instructions and test cases are two disjoint sets where each instruction can have multiple test cases,
and each test case can cover different DXIL instructions. So these two sets form a bipartite graph.

test_name: Test case identifier. Must be unique for each test case.
insts: dxil instructions
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

class test_case(object):
    def __init__(self, test_name, insts, validation_type, validation_tolerance,
                 input_lists, output_lists, shader_target, shader_text, **kwargs):
        self.test_name = test_name
        self.validation_type = validation_type
        self.validation_tolerance = validation_tolerance
        self.input_lists = input_lists
        self.output_lists = output_lists
        self.shader_target = shader_target
        self.shader_text = shader_text
        self.insts = insts # list of instructions each test case cover
        self.warp_version = -1 # known warp version that works
        for k,v in kwargs.items():
            setattr(self, k, v)

# Wrapper for each DXIL instruction
class inst_node(object):
    def __init__(self, inst):
        self.inst = inst
        self.test_cases = []  # list of test_case

def add_test_case(test_name, inst_names, validation_type, validation_tolerance,
                  input_lists, output_lists, shader_target, shader_text, **kwargs):
    insts = []
    for inst_name in inst_names:
        assert (inst_name in g_instruction_nodes)
        insts += [g_instruction_nodes[inst_name].inst]
    case = test_case(test_name, insts, validation_type,
                    validation_tolerance, input_lists, output_lists,
                    shader_target, shader_text, **kwargs)
    g_test_cases[test_name] = case
    # update instruction nodes
    for inst_name in inst_names:
        g_instruction_nodes[inst_name].test_cases += [case]


# This is a collection of test case for driver tests per instruction
# Warning: For test cases, when you want to pass in signed integer,
# make sure to pass in negative numbers with decimal values instead of hexadecimal representation.
# For some reason, TAEF is not handling them properly.
def add_test_cases():
    nan = float('nan')
    p_inf = float('inf')
    n_inf = float('-inf')
    p_denorm = float('1e-38')
    n_denorm = float('-1e-38')
    # Unary Float
    add_test_case('Sin', ['Sin'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16',
        '314.16'
    ]], [[
        'NaN', 'NaN', '-0', '-0', '0', '0', 'NaN', '-0.0007346401',
        '0.0007346401'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            [RootSignature("RootFlags(0), UAV(u0)")]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = sin(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Cos', ['Cos'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16',
        '314.16'
    ]], [[
        'NaN', 'NaN', '1.0', '1.0', '1.0', '1.0', 'NaN', '0.99999973015',
        '0.99999973015'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = cos(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Tan', ['Tan'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16',
        '314.16'
    ]], [[
        'NaN', 'NaN', '-0.0', '-0.0', '0.0', '0.0', 'NaN', '-0.000735',
        '0.000735'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = tan(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Hcos', ['Hcos'], 'Epsilon', 0.0008,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']], [[
            'NaN', 'Inf', '1.0', '1.0', '1.0', '1.0', 'Inf', '1.543081',
            '1.543081'
        ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = cosh(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Hsin', ['Hsin'], 'Epsilon', 0.0008,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']], [[
            'NaN', '-Inf', '0.0', '0.0', '0.0', '0.0', 'Inf', '1.175201',
            '-1.175201'
        ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = sinh(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Htan', ['Htan'], 'Epsilon', 0.0008,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']], [[
            'NaN', '-1', '-0.0', '-0.0', '0.0', '0.0', '1', '0.761594',
            '-0.761594'
        ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = tanh(l.input);
                g_buf[GI] = l;
            };''', warp_version=16202)
    add_test_case('Acos', ['Acos'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5',
        '-1.5'
    ]], [[
        'NaN', 'NaN', '1.570796', '1.570796', '1.570796', '1.570796', 'NaN',
        '0', '3.1415926', 'NaN', 'NaN'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = acos(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Asin', ['Asin'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5',
        '-1.5'
    ]], [[
        'NaN', 'NaN', '0.0', '0.0', '0.0', '0.0', 'NaN', '1.570796',
        '-1.570796', 'NaN', 'NaN'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = asin(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Atan', ['Atan'], 'Epsilon', 0.0008,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']], [[
            'NaN', '-1.570796', '0.0', '0.0', '0.0', '0.0', '1.570796',
            '0.785398163', '-0.785398163'
        ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = atan(l.input);
                g_buf[GI] = l;
            };''', warp_version=16202)
    add_test_case('Exp', ['Exp'], 'Relative', 21,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '10']],
        [['NaN', '0', '1', '1', '1', '1', 'Inf', '0.367879441', '22026.46579']
         ], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = exp(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Frc', ['Frc'], 'Epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2.718280',
        '1000.599976', '-7.389'
    ]], [[
        'NaN', 'NaN', '0', '0', '0', '0', 'NaN', '0', '0.718280', '0.599976',
        '0.611'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = frac(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Log', ['Log'], 'Relative', 21, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1',
        '2.718281828', '7.389056', '100'
    ]], [[
        'NaN', 'NaN', '-Inf', '-Inf', '-Inf', '-Inf', 'Inf', 'NaN', '1.0',
        '1.99999998', '4.6051701'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = log(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Sqrt', ['Sqrt'], 'ulp', 1, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2',
        '16.0', '256.0'
    ]], [[
        'NaN', 'NaN', '-0', '-0', '0', '0', 'Inf', 'NaN', '1.41421356237',
        '4.0', '16.0'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = sqrt(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Rsqrt', ['Rsqrt'], 'ulp', 1, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0',
        '256.0', '65536.0'
    ]], [[
        'NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25',
        '0.0625', '0.00390625'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = rsqrt(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Rsqrt', ['Rsqrt'], 'ulp', 1, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0',
        '256.0', '65536.0'
    ]], [[
        'NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25',
        '0.0625', '0.00390625'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = rsqrt(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Round_ne', ['Round_ne'], 'Epsilon', 0, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4',
        '10.5', '10.6', '11.5', '-10.0', '-10.4', '-10.5', '-10.6'
    ]], [[
        'NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0',
        '11.0', '12.0', '-10.0', '-10.0', '-10.0', '-11.0'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = round(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Round_ni', ['Round_ni'], 'Epsilon', 0, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4',
        '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6'
    ]], [[
        'NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0',
        '10.0', '-10.0', '-11.0', '-11.0', '-11.0'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = floor(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Round_pi', ['Round_pi'], 'Epsilon', 0, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4',
        '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6'
    ]], [[
        'NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '11.0', '11.0',
        '11.0', '-10.0', '-10.0', '-10.0', '-10.0'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = ceil(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Round_z', ['Round_z'], 'Epsilon', 0, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4',
        '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6'
    ]], [[
        'NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0',
        '10.0', '-10.0', '-10.0', '-10.0', '-10.0'
    ]], 'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = trunc(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('IsNaN', ['IsNaN'], 'Epsilon', 0,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']
         ], [['1', '0', '0', '0', '0', '0', '0', '0', '0']], 'cs_6_0',
        ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                if (isnan(l.input))
                    l.output = 1;
                else
                    l.output = 0;
                g_buf[GI] = l;
            };''')
    add_test_case('IsInf', ['IsInf'], 'Epsilon', 0,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']
         ], [['0', '1', '0', '0', '0', '0', '1', '0', '0']], 'cs_6_0',
        ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                if (isinf(l.input))
                    l.output = 1;
                else
                    l.output = 0;
                g_buf[GI] = l;
            };''')
    add_test_case('IsFinite', ['IsFinite'], 'Epsilon', 0,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']
         ], [['0', '0', '1', '1', '1', '1', '0', '1', '1']], 'cs_6_0',
        ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                if (isfinite(l.input))
                    l.output = 1;
                else
                    l.output = 0;
                g_buf[GI] = l;
            };''', warp_version=16202)
    add_test_case('FAbs', ['FAbs'], 'Epsilon', 0,
        [['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']
         ], [['NaN', 'Inf', 'denorm', '0', '0', 'denorm', 'Inf', '1', '1']],
        'cs_6_0', ''' struct SUnaryFPOp {
                float input;
                float output;
            };
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryFPOp l = g_buf[GI];
                l.output = abs(l.input);
                g_buf[GI] = l;
            };''')
    # Binary Float
    add_test_case('FMin', ['FMin','FMax'], 'epsilon', 0, [[
        '-inf', '-inf', '-inf', '-inf', 'inf', 'inf', 'inf', 'inf', 'NaN',
        'NaN', 'NaN', 'NaN', '1.0', '1.0', '-1.0', '-1.0', '1.0'
    ], [
        '-inf', 'inf', '1.0', 'NaN', '-inf', 'inf', '1.0', 'NaN', '-inf',
        'inf', '1.0', 'NaN', '-inf', 'inf', '1.0', 'NaN', '-1.0'
    ]], [[
        '-inf', '-inf', '-inf', '-inf', '-inf', 'inf', '1.0', 'inf', '-inf',
        'inf', '1.0', 'NaN', '-inf', '1.0', '-1.0', '-1.0', '-1.0'
    ], [
        '-inf', 'inf', '1.0', '-inf', 'inf', 'inf', 'inf', 'inf', '-inf',
        'inf', '1.0', 'NaN', '1.0', 'inf', '1.0', '-1.0', '1.0'
    ]], 'cs_6_0', ''' struct SBinaryFPOp {
                float input1;
                float input2;
                float output1;
                float output2;
            };
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryFPOp l = g_buf[GI];
                l.output1 = min(l.input1, l.input2);
                l.output2 = max(l.input1, l.input2);
                g_buf[GI] = l;
            };''')
    add_test_case('FAdd', ['FAdd'], 'ulp', 1, [['-1.0', '1.0', '32.5', '1.0000001000'],['4', '5.5', '334.7', '0.5000001000']], [['3.0', '6.5', '367.2', '1.5000002000']],
    'cs_6_0', ''' struct SBinaryFPOp {
                float input1;
                float input2;
                float output1;
                float output2;
            };
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryFPOp l = g_buf[GI];
                l.output1 = l.input1 + l.input2;
                g_buf[GI] = l;
            }; '''
    )
    add_test_case('FSub', ['FSub'], 'ulp', 1, [['-1.0', '5.5', '32.5', '1.0000001000'],['4', '1.25', '334.7', '0.5000001000']], [['-5', '4.25', '-302.2', '0.5000']],
    'cs_6_0', ''' struct SBinaryFPOp {
                float input1;
                float input2;
                float output1;
                float output2;
            };
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryFPOp l = g_buf[GI];
                l.output1 = l.input1 - l.input2;
                g_buf[GI] = l;
            }; '''
    )
    add_test_case('FMul', ['FMul'], 'ulp', 1, [['-1.0', '5.5', '1.0000001'],['4', '1.25', '2.0']], [['-4.0', '6.875', '2.0000002']],
    'cs_6_0', ''' struct SBinaryFPOp {
                float input1;
                float input2;
                float output1;
                float output2;
            };
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryFPOp l = g_buf[GI];
                l.output1 = l.input1 * l.input2;
                g_buf[GI] = l;
            }; '''
    )
    add_test_case('FDiv', ['FDiv'], 'ulp', 1, [['-1.0', '5.5', '1.0000001'],['4', '1.25', '2.0']], [['-0.25', '4.4', '0.50000006']],
    'cs_6_0', ''' struct SBinaryFPOp {
                float input1;
                float input2;
                float output1;
                float output2;
            };
            RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryFPOp l = g_buf[GI];
                l.output1 = l.input1 / l.input2;
                g_buf[GI] = l;
            }; '''
    )
    # Tertiary Float
    add_test_case('FMad', ['FMad'], 'epsilon', 0.0008, [[
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0',
        '0', '1', '1.5'
    ], [
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0',
        '0', '1', '10'
    ], [
        'NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0',
        '1', '0', '-5.5'
    ]], [['NaN', 'NaN', '0', '0', '0', '0', 'Inf', '2', '0', '1', '1', '9.5']],
                  'cs_6_0', ''' struct STertiaryFloatOp {
                    float input1;
                    float input2;
                    float input3;
                    float output;
                };
                RWStructuredBuffer<STertiaryFloatOp> g_buf : register(u0);
                [numthreads(8,8,1)]
                void main(uint GI : SV_GroupIndex) {
                    STertiaryFloatOp l = g_buf[GI];
                    l.output = mad(l.input1, l.input2, l.input3);
                    g_buf[GI] = l;
                };''')
    # Unary Int
    add_test_case('Bfrev', ['Bfrev'], 'Epsilon', 0, [[
        '-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536',
        '2147483647'
    ]], [[
        '1', '65535', '536870911', '-1', '0', '-2147483648', '268435456',
        '32768', '-2'
    ]], 'cs_6_0', ''' struct SUnaryIntOp {
                int input;
                int output;
            };
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryIntOp l = g_buf[GI];
                l.output = reversebits(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('FirstbitSHi', ['FirstbitSHi'], 'Epsilon', 0, [[
        '-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536',
        '2147483647'
    ]], [['30', '15', '2', '-1', '-1', '0', '3', '16', '30']], 'cs_6_0',
                  ''' struct SUnaryIntOp {
                int input;
                int output;
            };
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryIntOp l = g_buf[GI];
                l.output = firstbithigh(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('FirstbitLo', ['FirstbitLo'], 'Epsilon', 0, [[
        '-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536',
        '2147483647'
    ]], [['31', '16', '3', '0', '-1', '0', '3', '16', '0']], 'cs_6_0',
                  ''' struct SUnaryIntOp {
                int input;
                int output;
            };
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryIntOp l = g_buf[GI];
                l.output = firstbitlow(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('Countbits', ['Countbits'], 'Epsilon', 0, [[
        '-2147483648', '-65536', '-8', '-1', '0', '1', '8', '65536',
        '2147483647'
    ]], [['1', '16', '29', '32', '0', '1', '1', '1', '31']], 'cs_6_0',
                  ''' struct SUnaryIntOp {
                int input;
                int output;
            };
            RWStructuredBuffer<SUnaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryIntOp l = g_buf[GI];
                l.output = countbits(l.input);
                g_buf[GI] = l;
            };''')
    add_test_case('FirstbitHi', ['FirstbitHi'], 'Epsilon', 0,
                  [['0', '1', '8', '65536', '2147483647', '4294967295']],
                  [['-1', '0', '3', '16', '30', '31']], 'cs_6_0',
                  ''' struct SUnaryUintOp {
                uint input;
                uint output;
            };
            RWStructuredBuffer<SUnaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SUnaryUintOp l = g_buf[GI];
                l.output = firstbithigh(l.input);
                g_buf[GI] = l;
            };''')
    # Binary Int
    add_test_case('IAdd', ['Add'], 'Epsilon', 0,
                  [['-2147483648', '-10', '0', '0', '10', '2147483647', '486'],
                   ['0', '10', '-10', '10', '10', '0', '54238']],
                  [['-2147483648', '0', '-10', '10', '20', '2147483647', '54724']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = l.input1 + l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('ISub', ['Sub'], 'Epsilon', 0,
                  [['-2147483648', '-10', '0', '0', '10', '2147483647', '486'],
                   ['0', '10', '-10', '10', '10', '0', '54238']],
                  [['-2147483648', '-20', '10', '-10', '0', '2147483647', '-53752']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = l.input1 - l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('IMax', ['IMax'], 'Epsilon', 0,
                  [['-2147483648', '-10', '0', '0', '10', '2147483647'],
                   ['0', '10', '-10', '10', '10', '0']],
                  [['0', '10', '0', '10', '10', '2147483647']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = max(l.input1, l.input2);
                g_buf[GI] = l;
            };''')
    add_test_case('IMin', ['IMin'], 'Epsilon', 0,
                  [['-2147483648', '-10', '0', '0', '10', '2147483647'],
                   ['0', '10', '-10', '10', '10', '0']],
                  [['-2147483648', '-10', '-10', '0', '10', '0']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = min(l.input1, l.input2);
                g_buf[GI] = l;
            };''')
    add_test_case('IMul', ['Mul'], 'Epsilon', 0, [[
            '-2147483648', '-10', '-1', '0', '1', '10', '10000', '2147483647',
            '2147483647'
        ], ['-10', '-10', '10', '0', '256', '4', '10001', '0', '2147483647']],
        [['0', '100', '-10', '0', '256', '40', '100010000', '0', '1']],
        'cs_6_0', ''' struct SBinaryIntOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = l.input1 * l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('IDiv', ['SDiv', 'SRem'], 'Epsilon', 0,
        [['1', '1', '10', '10000', '2147483647', '2147483647', '-1'],
         ['1', '256', '4', '10001', '2', '2147483647', '1']],
        [['1', '0', '2', '0', '1073741823', '1', '-1'],
         ['0', '1', '2', '10000', '1', '0', '0']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 / l.input2;
                l.output2 = l.input1 % l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('Shl', ['Shl'], 'Epsilon', 0,
        [['1', '1', '0x1010', '0xa', '-1', '0x12341234', '-1'],
         ['0', '259', '4', '2', '0', '15', '3']],
        [['0x1', '0x8', '0x10100', '0x28', '-1','0x091a0000', '-8']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 << l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case("LShr", ['LShr'], 'Epsilon', 0,
        [['1', '1', '0xffff', '0x7fffffff', '0x70001234', '0x12340ab3', '0x7fffffff'],
        ['0', '1', '4', '30', '15', '16', '1']],
        [['1', '0', '0xfff', '1', '0xe000', '0x1234', '0x3fffffff']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 >> l.input2;
                g_buf[GI] = l;
            };'''
    )
    add_test_case("And", ['And'], 'Epsilon', 0,
        [['0x1', '0x01', '0x7fff0000', '0x33333333', '0x137f', '0x12345678', '0xa341', '-1'],
         ['0x1', '0xf0', '0x0000ffff', '0x22222222', '0xec80', '-1', '0x3471', '-1']],
        [['0x1', '0x00', '0x0', '0x22222222', '0x0', '0x12345678', '0x2041', '-1']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 & l.input2;
                g_buf[GI] = l;
            };'''
    )
    add_test_case("Or", ['Or'], 'Epsilon', 0,
        [['0x1', '0x01', '0x7fff0000', '0x11111111', '0x137f', '0x0', '0x12345678', '0xa341', '-1'],
         ['0x1', '0xf0', '0x0000ffff', '0x22222222', '0xec80', '0x0', '0x00000000', '0x3471', '-1']],
        [['0x1', '0xf1', '0x7fffffff', '0x33333333', '0xffff', '0x0', '0x12345678', '0xb771', '-1']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 | l.input2;
                g_buf[GI] = l;
            };'''
    )
    add_test_case("Xor", ['Xor'], 'Epsilon', 0,
        [['0x1', '0x01', '0x7fff0000', '0x11111111', '0x137f', '0x0', '0x12345678', '0xa341', '-1'],
         ['0x1', '0xf0', '0x0000ffff', '0x22222222', '0xec80', '0x0', '0x00000000', '0x3471', '-1']],
        [['0x0', '0xf1', '0x7fffffff', '0x33333333', '0xffff', '0x0', '0x12345678', '0x9730', '0x00000000']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                int input1;
                int input2;
                int output1;
                int output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 ^ l.input2;
                g_buf[GI] = l;
            };'''
    )

    # Binary Uint
    add_test_case('UAdd', ['Add'], 'Epsilon', 0,
                  [['2147483648', '4294967285', '0', '0', '10', '2147483647', '486'],
                   ['0', '10', '0', '10', '10', '0', '54238']],
                  [['2147483648', '4294967295', '0', '10', '20', '2147483647', '54724']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = l.input1 + l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('USub', ['Sub'], 'Epsilon', 0,
                  [['2147483648', '4294967295', '0', '0', '30', '2147483647', '54724'],
                   ['0', '10', '0', '10', '10', '0', '54238']],
                  [['2147483648', '4294967285', '0', '4294967286', '20', '2147483647', '486']], 'cs_6_0',
                  ''' struct SBinaryIntOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryIntOp l = g_buf[GI];
                l.output1 = l.input1 - l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('UMax', ['UMax'], 'Epsilon', 0,
                  [['0', '0', '10', '10000', '2147483647', '4294967295'],
                   ['0', '256', '4', '10001', '0', '4294967295']],
                  [['0', '256', '10', '10001', '2147483647', '4294967295']],
                  'cs_6_0', ''' struct SBinaryUintOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = max(l.input1, l.input2);
                g_buf[GI] = l;
            };''')
    add_test_case('UMin', ['UMin'], 'Epsilon', 0,
                  [['0', '0', '10', '10000', '2147483647', '4294967295'],
                   ['0', '256', '4', '10001', '0', '4294967295']],
                  [['0', '0', '4', '10000', '0', '4294967295']], 'cs_6_0',
                  ''' struct SBinaryUintOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = min(l.input1, l.input2);
                g_buf[GI] = l;
            };''')
    add_test_case('UMul', ['Mul'], 'Epsilon', 0,
                  [['0', '1', '10', '10000', '2147483647'],
                   ['0', '256', '4', '10001', '0']],
                  [['0', '256', '40', '100010000', '0']], 'cs_6_0',
                  ''' struct SBinaryUintOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 * l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('UDiv', ['UDiv', 'URem'], 'Epsilon', 0,
        [['1', '1', '10', '10000', '2147483647', '2147483647', '0xffffffff'],
         ['0', '256', '4', '10001', '0', '2147483647', '1']],
        [['0xffffffff', '0', '2', '0', '0xffffffff', '1', '0xffffffff'],
         ['0xffffffff', '1', '2', '10000', '0xffffffff', '0', '0']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                l.output1 = l.input1 / l.input2;
                l.output2 = l.input1 % l.input2;
                g_buf[GI] = l;
            };''')
    add_test_case('UAddc', ['UAddc'], 'Epsilon', 0,
        [['1', '1', '10000', '0x80000000', '0x7fffffff', '0xffffffff'],
         ['0', '256', '10001', '1', '0x7fffffff', '0x7fffffff']],
        [['2', '2', '20000', '0', '0xfffffffe', '0xfffffffe'],
         ['0', '512', '20002', '3', '0xfffffffe', '0xffffffff']], 'cs_6_0',
        ''' struct SBinaryUintOp {
                uint input1;
                uint input2;
                uint output1;
                uint output2;
            };
            RWStructuredBuffer<SBinaryUintOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                SBinaryUintOp l = g_buf[GI];
                uint2 x = uint2(l.input1, l.input2);
                uint2 y = AddUint64(x, x);
                l.output1 = y.x;
                l.output2 = y.y;
                g_buf[GI] = l;
            };''')

    # Tertiary Int
    add_test_case('IMad', ['IMad'], 'epsilon', 0, [[
        '-2147483647', '-256', '-1', '0', '1', '2', '16', '2147483647', '1',
        '-1', '1', '10'
    ], ['1', '-256', '-1', '0', '1', '3', '16', '0', '1', '-1', '10', '100'], [
        '0', '0', '0', '0', '1', '3', '1', '255', '2147483646', '-2147483647',
        '-10', '-2000'
    ]], [[
        '-2147483647', '65536', '1', '0', '2', '9', '257', '255', '2147483647',
        '-2147483646', '0', '-1000'
    ]], 'cs_6_0', ''' struct STertiaryIntOp {
                int input1;
                int input2;
                int input3;
                int output;
            };
            RWStructuredBuffer<STertiaryIntOp> g_buf : register(u0);
            [numthreads(8,8,1)]
            void main(uint GI : SV_GroupIndex) {
                STertiaryIntOp l = g_buf[GI];
                l.output = mad(l.input1, l.input2, l.input3);
                g_buf[GI] = l;
            };''')

    add_test_case('UMad', ['UMad'], 'epsilon', 0,
                  [['0', '1', '2', '16', '2147483647', '0', '10'], [
                      '0', '1', '2', '16', '1', '0', '10'
                  ], ['0', '0', '1', '15', '0', '10', '10']],
                  [['0', '1', '5', '271', '2147483647', '10', '110']],
                  'cs_6_0', ''' struct STertiaryUintOp {
                    uint input1;
                    uint input2;
                    uint input3;
                    uint output;
                };
                RWStructuredBuffer<STertiaryUintOp> g_buf : register(u0);
                [numthreads(8,8,1)]
                void main(uint GI : SV_GroupIndex) {
                    STertiaryUintOp l = g_buf[GI];
                    l.output = mad(l.input1, l.input2, l.input3);
                    g_buf[GI] = l;
                };''')

    # Dot
    add_test_case('Dot', ['Dot2', 'Dot3', 'Dot4'], 'epsilon', 0.008, [[
        'NaN,NaN,NaN,NaN', '-Inf,-Inf,-Inf,-Inf',
        '-denorm,-denorm,-denorm,-denorm', '-0,-0,-0,-0', '0,0,0,0',
        'denorm,denorm,denorm,denorm', 'Inf,Inf,Inf,Inf', '1,1,1,1',
        '-10,0,0,10', 'Inf,Inf,Inf,-Inf'
    ], [
        'NaN,NaN,NaN,NaN', '-Inf,-Inf,-Inf,-Inf',
        '-denorm,-denorm,-denorm,-denorm', '-0,-0,-0,-0', '0,0,0,0',
        'denorm,denorm,denorm,denorm', 'Inf,Inf,Inf,Inf', '1,1,1,1',
        '10,0,0,10', 'Inf,Inf,Inf,Inf'
    ]], [
        [nan, p_inf, 0, 0, 0, 0, p_inf, 2, -100, p_inf],
        [nan, p_inf, 0, 0, 0, 0, p_inf, 3, -100, p_inf],
        [nan, p_inf, 0, 0, 0, 0, p_inf, 4, 0, nan],
    ], 'cs_6_0', ''' struct SDotOp {
                   float4 input1;
                   float4 input2;
                   float o_dot2;
                   float o_dot3;
                   float o_dot4;
                };
                RWStructuredBuffer<SDotOp> g_buf : register(u0);
                [numthreads(8,8,1)]
                void main(uint GI : SV_GroupIndex) {
                    SDotOp l = g_buf[GI];
                    l.o_dot2 = dot(l.input1.xy, l.input2.xy);
                    l.o_dot3 = dot(l.input1.xyz, l.input2.xyz);
                    l.o_dot4 = dot(l.input1.xyzw, l.input2.xyzw);
                    g_buf[GI] = l;
                };''')
    # Quaternary
    # Msad4 intrinsic calls both Bfi and Msad. Currently this is the only way to call bfi instruction from HLSL
    add_test_case('Bfi', ['Bfi', 'Msad'], 'epsilon', 0,
        [["0xA100B2C3", "0x00000000", "0xFFFF01C1", "0xFFFFFFFF"], [
            "0xD7B0C372, 0x4F57C2A3", "0xFFFFFFFF, 0x00000000",
            "0x38A03AEF, 0x38194DA3", "0xFFFFFFFF, 0x00000000"
        ], ["1,2,3,4", "1,2,3,4", "0,0,0,0", "10,10,10,10"]],
        [['153,6,92,113', '1,2,3,4', '397,585,358,707', '10,265,520,775']],
        'cs_6_0', ''' struct SMsad4 {
                        uint ref;
                        uint2 source;
                        uint4 accum;
                        uint4 result;
                    };
                    RWStructuredBuffer<SMsad4> g_buf : register(u0);
                    [numthreads(8,8,1)]
                    void main(uint GI : SV_GroupIndex) {
                        SMsad4 l = g_buf[GI];
                        l.result = msad4(l.ref, l.source, l.accum);
                        g_buf[GI] = l;
                    };''')

    # Wave Active Tests
    add_test_case('WaveActiveSum', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['2', '4', '8', '-64']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveSum(pts.input);
                        }
                        else {
                            pts.output = WaveActiveSum(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveProduct', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['1', '2', '4', '-64']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveProduct(pts.input);
                        }
                        else {
                            pts.output = WaveActiveProduct(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')

    add_test_case('WaveActiveCountBits', ['WaveAllBitCount', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['1', '10', '-4', '-64'],
                   ['-100', '-1000', '300']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveCountBits(pts.input > 3);
                        }
                        else {
                            pts.output = WaveActiveCountBits(pts.input > 3);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveMax', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['1', '10', '-4', '-64'],
                   ['-100', '-1000', '300']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveMax(pts.input);
                        }
                        else {
                            pts.output = WaveActiveMax(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveMin', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4', '5', '6', '7', '8', '9', '10'], ['0'],
                   ['1', '10', '-4', '-64'], ['-100', '-1000', '300']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveMin(pts.input);
                        }
                        else {
                            pts.output = WaveActiveMin(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveAllEqual', ['WaveActiveAllEqual'], 'Epsilon', 0,
                  [['1', '2', '3', '4', '1', '1', '1', '1'], ['3'], ['-10']],
                  [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveAllEqual(pts.input);
                        }
                        else {
                            pts.output = WaveActiveAllEqual(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveAnyTrue', ['WaveAnyTrue', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '0', '1', '0', '1'], ['1'], ['0']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        bool input;
                        bool output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveAnyTrue(pts.input);
                        }
                        else {
                            pts.output = WaveActiveAnyTrue(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveAllTrue', ['WaveAllTrue', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '0', '1', '0', '1'], ['1'], ['1']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        bool input;
                        bool output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveAllTrue(pts.input);
                        }
                        else {
                            pts.output = WaveActiveAllTrue(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')

    add_test_case('WaveActiveUSum', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['2', '4', '8', '64']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveSum(pts.input);
                        }
                        else {
                            pts.output = WaveActiveSum(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveUProduct', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['1', '2', '4', '64']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveProduct(pts.input);
                        }
                        else {
                            pts.output = WaveActiveProduct(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveUMax', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4'], ['0'], ['1', '10', '4', '64']], [],
                  'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveMax(pts.input);
                        }
                        else {
                            pts.output = WaveActiveMax(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveUMin', ['WaveActiveOp', 'WaveReadLaneFirst', 'WaveReadLaneAt'], 'Epsilon', 0,
                  [['1', '2', '3', '4', '5', '6', '7', '8', '9', '10'], ['0'],
                   ['1', '10', '4', '64']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveMin(pts.input);
                        }
                        else {
                            pts.output = WaveActiveMin(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveBitOr', ['WaveActiveBit'], 'Epsilon', 0, [[
        '0xe0000000', '0x0d000000', '0x00b00000', '0x00070000', '0x0000e000',
        '0x00000d00', '0x000000b0', '0x00000007'
    ], ['0xedb7edb7', '0xdb7edb7e', '0xb7edb7ed', '0x7edb7edb'], [
        '0x12481248', '0x24812481', '0x48124812', '0x81248124'
    ], ['0x00000000', '0xffffffff']], [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveBitOr(pts.input);
                        }
                        else {
                            pts.output = WaveActiveBitOr(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveBitAnd', ['WaveActiveBit'], 'Epsilon', 0, [[
        '0xefffffff', '0xfdffffff', '0xffbfffff', '0xfff7ffff', '0xffffefff',
        '0xfffffdff', '0xffffffbf', '0xfffffff7'
    ], ['0xedb7edb7', '0xdb7edb7e', '0xb7edb7ed', '0x7edb7edb'], [
        '0x12481248', '0x24812481', '0x48124812', '0x81248124'
    ], ['0x00000000', '0xffffffff']], [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveBitAnd(pts.input);
                        }
                        else {
                            pts.output = WaveActiveBitAnd(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WaveActiveBitXor', ['WaveActiveBit'], 'Epsilon', 0, [[
        '0xe0000000', '0x0d000000', '0x00b00000', '0x00070000', '0x0000e000',
        '0x00000d00', '0x000000b0', '0x00000007'
    ], ['0xedb7edb7', '0xdb7edb7e', '0xb7edb7ed', '0x7edb7edb'], [
        '0x12481248', '0x24812481', '0x48124812', '0x81248124'
    ], ['0x00000000', '0xffffffff']], [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WaveActiveBitXor(pts.input);
                        }
                        else {
                            pts.output = WaveActiveBitXor(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WavePrefixCountBits', ['WavePrefixBitCount'], 'Epsilon', 0,
                  [['1', '2', '3', '4', '5'], ['0'], ['1', '10', '-4', '-64'],
                   ['-100', '-1000', '300']], [], 'cs_6_0',
                  ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WavePrefixCountBits(pts.input > 3);
                        }
                        else {
                            pts.output = WavePrefixCountBits(pts.input > 3);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WavePrefixSum', ['WavePrefixOp'], 'Epsilon', 0,
        [['1', '2', '3', '4', '5'], ['0', '1'], ['1', '2', '4', '-64', '128']],
        [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WavePrefixSum(pts.input);
                        }
                        else {
                            pts.output = WavePrefixSum(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WavePrefixProduct', ['WavePrefixOp'], 'Epsilon', 0,
        [['1', '2', '3', '4', '5'], ['0', '1'], ['1', '2', '4', '-64', '128']],
        [], 'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        int input;
                        int output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WavePrefixProduct(pts.input);
                        }
                        else {
                            pts.output = WavePrefixProduct(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WavePrefixUSum', ['WavePrefixOp'], 'Epsilon', 0,
        [['1', '2', '3', '4', '5'], ['0', '1'], ['1', '2', '4', '128']], [],
        'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WavePrefixSum(pts.input);
                        }
                        else {
                            pts.output = WavePrefixSum(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')
    add_test_case('WavePrefixUProduct', ['WavePrefixOp'], 'Epsilon', 0,
        [['1', '2', '3', '4', '5'], ['0', '1'], ['1', '2', '4', '128']], [],
        'cs_6_0', ''' struct PerThreadData {
                        uint firstLaneId;
                        uint laneIndex;
                        int mask;
                        uint input;
                        uint output;
                    };
                    RWStructuredBuffer<PerThreadData> g_sb : register(u0);
                    [numthreads(8,12,1)]
                    void main(uint GI : SV_GroupIndex) {
                        PerThreadData pts = g_sb[GI];
                        pts.firstLaneId = WaveReadLaneFirst(GI);
                        pts.laneIndex = WaveGetLaneIndex();
                        if (pts.mask != 0) {
                            pts.output = WavePrefixProduct(pts.input);
                        }
                        else {
                            pts.output = WavePrefixProduct(pts.input);
                        }
                        g_sb[GI] = pts;
                    };''')


# generating xml file for execution test using data driven method
# TODO: ElementTree is not generating formatted XML. Currently xml file is checked in after VS Code formatter.
# Implement xml formatter or import formatter library and use that instead.

def generate_parameter_types(table, num_inputs, num_outputs, has_known_warp_issue=False):
    param_types = ET.SubElement(table, "ParameterTypes")
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "ShaderOp.Target"
        }).text = "String"
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "ShaderOp.Text"
        }).text = "String"
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "Validation.Type"
        }).text = "String"
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "Validation.Tolerance"
        }).text = "double"
    for i in range(0, num_inputs):
        ET.SubElement(
            param_types,
            "ParameterType",
            attrib={
                "Name": 'Validation.Input{}'.format(i + 1),
                'Array': 'true'
            }).text = "String"
    for i in range(0, num_outputs):
        ET.SubElement(
            param_types,
            "ParameterType",
            attrib={
                "Name": 'Validation.Expected{}'.format(i + 1),
                'Array': 'true'
            }).text = "String"
    if has_known_warp_issue:
        ET.SubElement(param_types, "ParameterType", attrib={"Name":"Warp.Version"}).text = "unsigned int"

def generate_parameter_types_wave(table):
    param_types = ET.SubElement(table, "ParameterTypes")
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "ShaderOp.Target"
        }).text = "String"
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "ShaderOp.Text"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.NumInputSet"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.InputSet1",
            "Array": "true"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.InputSet2",
            "Array": "true"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.InputSet3",
            "Array": "true"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.InputSet4",
            "Array": "true"
        }).text = "String"


def generate_parameter_types_msad(table):
    param_types = ET.SubElement(table, "ParameterTypes")
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "ShaderOp.Text"
        }).text = "String"
    ET.SubElement(
        param_types, "ParameterType", attrib={
            "Name": "Validation.Tolerance"
        }).text = "int"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.Input1",
            "Array": "true"
        }).text = "unsigned int"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.Input2",
            "Array": "true"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.Input3",
            "Array": "true"
        }).text = "String"
    ET.SubElement(
        param_types,
        "ParameterType",
        attrib={
            "Name": "Validation.Expected1",
            "Array": "true"
        }).text = "String"


def generate_row(table, case):
    row = ET.SubElement(table, "Row", {"Name": case.test_name})
    ET.SubElement(row, "Parameter", {
        "Name": "Validation.Type"
    }).text = case.validation_type
    ET.SubElement(row, "Parameter", {
        "Name": "Validation.Tolerance"
    }).text = str(case.validation_tolerance)
    ET.SubElement(row, "Parameter", {
        "Name": "ShaderOp.Text"
    }).text = case.shader_text
    ET.SubElement(row, "Parameter", {
        "Name": "ShaderOp.Target"
    }).text = case.shader_target
    for i in range(len(case.input_lists)):
        inputs = ET.SubElement(row, "Parameter", {
            "Name": "Validation.Input{}".format(i + 1)
        })
        for val in case.input_lists[i]:
            ET.SubElement(inputs, "Value").text = str(val)
    for i in range(len(case.output_lists)):
        outputs = ET.SubElement(row, "Parameter", {
            "Name": "Validation.Expected{}".format(i + 1)
        })
        for val in case.output_lists[i]:
            ET.SubElement(outputs, "Value").text = str(val)
    if case.warp_version > 0:
        ET.SubElement(row, "Parameter", {"Name":"Warp.Version"}).text = str(case.warp_version)

def generate_row_wave(table, case):
    row = ET.SubElement(table, "Row", {"Name": case.test_name})
    ET.SubElement(row, "Parameter", {
        "Name": "ShaderOp.Name"
    }).text = case.test_name
    ET.SubElement(row, "Parameter", {
        "Name": "ShaderOp.Text"
    }).text = case.shader_text
    ET.SubElement(row, "Parameter", {
        "Name": "Validation.NumInputSet"
    }).text = str(len(case.input_lists))
    for i in range(len(case.input_lists)):
        inputs = ET.SubElement(row, "Parameter", {
            "Name": "Validation.InputSet{}".format(i + 1)
        })
        for val in case.input_lists[i]:
            ET.SubElement(inputs, "Value").text = str(val)


def generate_table_for_taef():
    with open("..\\..\\tools\\clang\\unittests\\HLSL\\ShaderOpArithTable.xml",
              'w') as f:
        tree = ET.ElementTree()
        root = ET.Element('Data')
        # Create tables
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "UnaryFloatOpTable"
            }), 1, 1, True)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "BinaryFloatOpTable"
            }), 2, 2)
        generate_parameter_types(
            ET.SubElement(
                root, "Table", attrib={
                    "Id": "TertiaryFloatOpTable"
                }), 3, 1)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "UnaryIntOpTable"
            }), 1, 1)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "BinaryIntOpTable"
            }), 2, 2)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "TertiaryIntOpTable"
            }), 3, 1)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "UnaryUintOpTable"
            }), 1, 1)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "BinaryUintOpTable"
            }), 2, 2)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "TertiaryUintOpTable"
            }), 3, 1)
        generate_parameter_types(
            ET.SubElement(root, "Table", attrib={
                "Id": "DotOpTable"
            }), 2, 3)
        generate_parameter_types_msad(
            ET.SubElement(root, "Table", attrib={
                "Id": "Msad4Table"
            }))
        generate_parameter_types_wave(
            ET.SubElement(
                root, "Table", attrib={
                    "Id": "WaveIntrinsicsActiveIntTable"
                }))
        generate_parameter_types_wave(
            ET.SubElement(
                root, "Table", attrib={
                    "Id": "WaveIntrinsicsActiveUintTable"
                }))
        generate_parameter_types_wave(
            ET.SubElement(
                root, "Table", attrib={
                    "Id": "WaveIntrinsicsPrefixIntTable"
                }))
        generate_parameter_types_wave(
            ET.SubElement(
                root, "Table", attrib={
                    "Id": "WaveIntrinsicsPrefixUintTable"
                }))

        for case in g_test_cases.values():
            cur_inst = case.insts[0]
            if cur_inst.is_cast or cur_inst.category.startswith("Unary"):
                if "f" in cur_inst.oload_types:
                    generate_row(
                        root.find("./Table[@Id='UnaryFloatOpTable']"),
                        case)
                if "i" in cur_inst.oload_types:
                    if cur_inst.category.startswith("Unary int"):
                        generate_row(
                            root.find("./Table[@Id='UnaryIntOpTable']"),
                            case)
                    elif cur_inst.category.startswith("Unary uint"):
                        generate_row(
                            root.find("./Table[@Id='UnaryUintOpTable']"),
                            case)
                    else:
                        print("unknown op: " + cur_inst.name)
                        print(cur_inst.dxil_class)
            elif cur_inst.is_binary or cur_inst.category.startswith(
                    "Binary"):
                if "f" in cur_inst.oload_types:
                    generate_row(
                        root.find("./Table[@Id='BinaryFloatOpTable']"),
                        case)
                elif "i" in cur_inst.oload_types:
                    if cur_inst.category.startswith("Binary int"):
                        if case.test_name in ['UAdd', 'USub', 'UMul']: # Add, Sub, Mul use same operations for int and uint.
                            generate_row(
                                root.find("./Table[@Id='BinaryUintOpTable']"),
                                case)
                        else:
                            generate_row(
                                root.find("./Table[@Id='BinaryIntOpTable']"),
                                case)
                    elif cur_inst.category.startswith("Binary uint"):
                        generate_row(
                            root.find("./Table[@Id='BinaryUintOpTable']"),
                            case)
                    else:
                        print("unknown op: " + cur_inst.name)
                        print(cur_inst.dxil_class)

            elif cur_inst.category.startswith("Tertiary"):
                if "f" in cur_inst.oload_types:
                    generate_row(
                        root.find("./Table[@Id='TertiaryFloatOpTable']"),
                        case)
                elif "i" in cur_inst.oload_types:
                    if cur_inst.category.startswith("Tertiary int"):
                        generate_row(
                            root.find("./Table[@Id='TertiaryIntOpTable']"),
                            case)
                    elif cur_inst.category.startswith("Tertiary uint"):
                        generate_row(
                            root.find(
                                "./Table[@Id='TertiaryUintOpTable']"),
                            case)
                    else:
                        print("unknown op: " + cur_inst.name)
                        print(cur_inst.dxil_class)
                else:
                    print("unknown op: " + cur_inst.name)
                    print(cur_inst.dxil_class)
            elif cur_inst.category.startswith("Quaternary"):
                if cur_inst.name == "Bfi":
                    generate_row(
                        root.find("./Table[@Id='Msad4Table']"), case)
                else:
                    print("unknown op: " + cur_inst.name)
                    print(cur_inst.dxil_class)
            elif cur_inst.category == "Dot":
                generate_row(root.find("./Table[@Id='DotOpTable']"), case)
            elif cur_inst.dxil_class in ["WaveActiveOp", "WaveAllOp","WaveActiveAllEqual","WaveAnyTrue","WaveAllTrue"]:
                if case.test_name.startswith("WaveActiveU"):
                    generate_row_wave(
                        root.find(
                            "./Table[@Id='WaveIntrinsicsActiveUintTable']"
                        ), case)
                else:
                    generate_row_wave(
                        root.find(
                            "./Table[@Id='WaveIntrinsicsActiveIntTable']"),
                        case)
            elif cur_inst.dxil_class == "WaveActiveBit":
                generate_row_wave(
                    root.find(
                        "./Table[@Id='WaveIntrinsicsActiveUintTable']"),
                    case)
            elif cur_inst.dxil_class == "WavePrefixOp":
                if case.test_name.startswith("WavePrefixU"):
                    generate_row_wave(
                        root.find(
                            "./Table[@Id='WaveIntrinsicsPrefixUintTable']"
                        ), case)
                else:
                    generate_row_wave(
                        root.find(
                            "./Table[@Id='WaveIntrinsicsPrefixIntTable']"),
                        case)
            else:
                print("unknown op: " + cur_inst.name)
                print(cur_inst.dxil_class)
        tree._setroot(root)
        tree.write(f)
        f.close()

def print_untested_inst():
    lst = []
    for name in [node.inst.name for node in g_instruction_nodes.values() if len(node.test_cases) == 0]:
        lst += [name]
    lst.sort()
    for name in lst:
        print(name)
    print("Total uncovered dxil ops: " + str(len(lst)))
    print("Total covered dxil ops: " + str(len(g_instruction_nodes)-len(lst)))


# name to instruction dict
g_instruction_nodes = {}
# test name to test case dict
g_test_cases = {}

if __name__ == "__main__":
    db = get_db_dxil()
    for inst in db.instr:
        g_instruction_nodes[inst.name] = inst_node(inst)
    add_test_cases()

    args = vars(parser.parse_args())
    mode = args['mode']
    if mode == "info":
        print_untested_inst()
    elif mode == "gen-xml":
        generate_table_for_taef()
    else:
        print("unknown mode: " + mode)
        exit(1)
    exit(0)
