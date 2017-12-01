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

def update_test_case(name, validation_type, validation_tolerance, input_lists, output_lists, shader_target, shader_text):
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
    update_test_case('Sin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '-0', '-0', '0', '0', 'NaN', '-0.0007346401', '0.0007346401']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            [RootSignature("RootFlags(0), UAV(u0)")]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = sin(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Cos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '1.0', '1.0', '1.0', '1.0', 'NaN', '0.99999973015', '0.99999973015']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = cos(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Tan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-314.16', '314.16']],[['NaN', 'NaN', '-0.0', '-0.0', '0.0', '0.0', 'NaN', '-0.000735', '0.000735']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = tan(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Hcos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', 'Inf', '1.0', '1.0', '1.0', '1.0', 'Inf', '1.543081', '1.543081']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = cosh(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Hsin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-Inf', '0.0', '0.0', '0.0', '0.0', 'Inf', '1.175201', '-1.175201']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = sinh(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Htan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-1', '-0.0', '-0.0', '0.0', '0.0', '1', '0.761594', '-0.761594']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = tanh(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Acos', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5', '-1.5']],[['NaN', 'NaN', '1.570796', '1.570796', '1.570796', '1.570796', 'NaN', '0', '3.1415926', 'NaN', 'NaN']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = acos(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Asin', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1', '1.5', '-1.5']],[['NaN', 'NaN', '0.0', '0.0', '0.0', '0.0', 'NaN', '1.570796', '-1.570796', 'NaN', 'NaN']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = asin(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Atan', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1', '-1']],[['NaN', '-1.570796', '0.0', '0.0', '0.0', '0.0', '1.570796', '0.785398163', '-0.785398163']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = atan(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Exp', 'Relative',21,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '10']],[['NaN', '0', '1', '1', '1', '1', 'Inf', '0.367879441', '22026.46579']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = exp(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Frc', 'Epsilon',0.0008,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2.718280', '1000.599976', '-7.389']],[['NaN', 'NaN', '0', '0', '0', '0', 'NaN', '0', '0.718280', '0.599976', '0.611']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = frac(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Log', 'Relative',21,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2.718281828', '7.389056', '100']],[['NaN', 'NaN', '-Inf', '-Inf', '-Inf', '-Inf', 'Inf', 'NaN', '1.0', '1.99999998', '4.6051701']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = log(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Sqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '2', '16.0', '256.0']],[['NaN', 'NaN', '-0', '-0', '0', '0', 'Inf', 'NaN', '1.41421356237', '4.0', '16.0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = sqrt(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Rsqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0', '256.0', '65536.0']],[['NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25', '0.0625', '0.00390625']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = rsqrt(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Rsqrt', 'ulp',1,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '-1', '16.0', '256.0', '65536.0']],[['NaN', 'NaN', '-Inf', '-Inf', 'Inf', 'Inf', '0', 'NaN', '0.25', '0.0625', '0.00390625']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = rsqrt(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Round_ne', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '11.5', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '11.0', '12.0', '-10.0', '-10.0', '-10.0', '-11.0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = round(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Round_ni', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '10.0', '-10.0', '-11.0', '-11.0', '-11.0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = floor(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Round_pi', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '11.0', '11.0', '11.0', '-10.0', '-10.0', '-10.0', '-10.0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = ceil(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('Round_z', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '10.0', '10.4', '10.5', '10.6', '-10.0', '-10.4', '-10.5', '-10.6']],[['NaN', '-Inf', '-0', '-0', '0', '0', 'Inf', '10.0', '10.0', '10.0', '10.0', '-10.0', '-10.0', '-10.0', '-10.0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = trunc(l.input);\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('IsNaN', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['1', '0', '0', '0', '0', '0', '0', '0', '0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                if (isnan(l.input))\n\
                    l.output = 1;\n\
                else\n\
                    l.output = 0;\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('IsInf', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['0', '1', '0', '0', '0', '0', '1', '0', '0']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                if (isinf(l.input))\n\
                    l.output = 1;\n\
                else\n\
                    l.output = 0;\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('IsFinite', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['0', '0', '1', '1', '1', '1', '0', '1', '1']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                if (isfinite(l.input))\n\
                    l.output = 1;\n\
                else\n\
                    l.output = 0;\n\
                g_buf[GI] = l;\n\
            }')
    update_test_case('FAbs', 'Epsilon',0,[['NaN', '-Inf', '-denorm', '-0', '0', 'denorm', 'Inf', '1.0', '-1.0']],[['NaN', 'Inf', 'denorm', '0', '0', 'denorm', 'Inf', '1', '1']],'cs_6_0',
    'struct SUnaryFPOp {\n\
                float input;\n\
                float output;\n\
            };\n\
            RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
            [numthreads(8,8,1)]\n\
            void main(uint GI : SV_GroupIndex) {\n\
                SUnaryFPOp l = g_buf[GI];\n\
                l.output = abs(l.input);\n\
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

# Msad4 intrinsic calls both Bfi and Msad
def generate_parameter_types_msad(table):
   param_types = ET.SubElement(table, "ParameterTypes")
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"ShaderOp.Text"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Tolerance"}).text = "int"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input1", "Array":"true"}).text = "unsigned int"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input2", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Input3", "Array":"true"}).text = "String"
   ET.SubElement(param_types, "ParameterType", attrib={"Name":"Validation.Expected", "Array":"true"}).text = "String"

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
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"BinaryFloatOpTable"}), 2, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"TertiaryFloatOpTable"}), 3, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"UnaryIntOpTable"}), 1, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"UnaryUintOpTable"}), 1, 1)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"BinaryIntOpTable"}), 2, 2)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"BinaryUintOpTable"}), 2, 2)
        generate_parameter_types(ET.SubElement(root, "Table", attrib={"Id":"DotOpTable"}), 2, 3)
        generate_parameter_types_msad(ET.SubElement(root, "Table", attrib={"Id":"MsadTable"}))

        for case in [x for x in g_instructions.values() if x.test_enabled]:
            if case.inst.is_binary or case.inst.category.startswith("Binary"):
                if "f" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='BinaryFloatOpTable']"), case)
            elif case.inst.is_cast or case.inst.category.startswith("Unary"):
                if "f" in case.inst.oload_types:
                    generate_row(root.find("./Table[@Id='UnaryFloatOpTable']"), case)

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