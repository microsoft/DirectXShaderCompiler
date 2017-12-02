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

def update_test_case(name, validation_type, validation_tolerance, inputs, outputs, shader_target, shader_text):
    g_instructions[name].inputs = inputs
    g_instructions[name].outputs = outputs
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
   update_test_case("Sin", "epsilon", 0.0008,
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -314.16, 314.16],
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -0.0007346401, 0.0007346401],
                    "cs_6_0", "\n\
                    struct SUnaryFPOp {\n\
                        float input;\n\
                        float output;\n\
                    };\n\
                    RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);!\n\
                    [numthreads(8,8,1)]\n\
                    void main(uint GI : SV_GroupIndex) {\n\
                        SUnaryFPOp l = g_buf[GI];\n\
                        l.output = sin(l.input);\n\
                        g_buf[GI] = l;\n\
                    }")

   update_test_case("Cos", "epsilon", 0.0008,
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -314.16, 314.16],
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, 0.99999973015, 0.99999973015],
                    "cs_6_0", 
                    "struct SUnaryFPOp {\n\
                    float input;\n\
                    float output;\n\
                    };\n\
                    RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\n\
                    [numthreads(8,8,1)]\n\
                    [RootSignature(\"RootFlags(0), UAV(u0)\")]\n\
                    void main(uint GI : SV_GroupIndex) {\n\
                        SUnaryFPOp l = g_buf[GI];\n\
                        l.output = cos(l.input);\n\
                        g_buf[GI] = l;\n\
                    }")
   # Also tests FMax
   update_test_case("FMin", "epsilon", 0,
        [(n_inf, n_inf), (n_inf, p_inf), (n_inf, 1.0), (n_inf, nan), (p_inf, n_inf), (p_inf, p_inf), (p_inf, 1.0), (p_inf, nan), (nan, n_inf), (nan, p_inf), (nan, 1.0), (nan, nan),(1.0, n_inf), (1.0, p_inf), (-1.0, 1.0), (-1.0, nan), (1.0, -1.0)],
        [n_inf, n_inf, n_inf, n_inf, n_inf, p_inf, 1.0, p_inf, -p_inf, p_inf, 1.0, nan, n_inf, 1.0, -1.0, -1.0, -1.0], 
                "cs_6_0", 
                "struct SBinaryFPOp {\n\
                    float input1;\n\
                    float input2;\n\
                    float output1;\n\
                    float output2;\n\
                };\n\
                RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);\n\
                [numthreads(8,8,1)]\n\
                void main(uint GI : SV_GroupIndex) {\n\
                    SBinaryFPOp l = g_buf[GI];\n\
                    l.output1 = min(l.input1, l.input2);\n\
                    l.output2 = max(l.input1, l.input2);\n\
                    g_buf[GI] = l;\n\
                };")

   update_test_case("FAdd", "ulp", 1, 
        [(125, 125), (250, 250)],[250, 500],"cs_6_0", 
                "struct SBinaryFPOp {\n\
                    float input1;\n\
                    float input2;\n\
                    float output1;\n\
                };\n\
                RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);\n\
                [numthreads(8,8,1)]\n\
                void main(uint GI : SV_GroupIndex) {\n\
                    SBinaryFPOp l = g_buf[GI];\n\
                    l.output1 = l.input1 + l.input2;\n\
                    g_buf[GI] = l;\n\
                };")

def find_inputs(row):
    inputs = []
    for name in ['Validation.Input', 'Validation.Input1', 'Validation.Input2', 'Validation.Input3']:
        input_set = []
        for value in row.findall("./Parameter[@Name='{}']/Value".format(name)):
            input_set += [value.text]
        if len(input_set) != 0: inputs.append(input_set)
    return inputs

def find_outputs(row):
    outputs = []
    for name in ['Validation.Expected', 'Validation.Expected1', 'Validation.Expected2']:
        output_set = []
        for value in row.findall("./Parameter[@Name='{}']/Value".format(name)):
            output_set += [value.text]
        if len(output_set) != 0: outputs.append(output_set) 
    return outputs

def extract():
   tree = ET.parse("..\\..\\tools\\clang\\unittests\\HLSL\\ShaderOpArithTable.xml")
   root = tree.getroot()
   num = 0
   with open("result.txt", 'w') as f:
    for table in root.iterfind("./Table"):
        for row in table.iterfind("./Row"):
            line = "    update_test_case('"+ row.get('Name').capitalize() +"', '"
            if row.find("./Parameter[@Name='Validation.Type']") is not None:
                line += row.find("./Parameter[@Name='Validation.Type']").text
            line += "',"
            if row.find("./Parameter[@Name='Validation.Tolerance']") != None:
                line += row.find("./Parameter[@Name='Validation.Tolerance']").text
            line += "," 
            inputs = find_inputs(row)
            
            line += str(inputs)
            line += ","
            outputs = find_outputs(row)
            line += str(outputs)
            line += ","
            #line += row.find("./Parameter[@Name='ShaderOp.Name']").text
            line += "'cs_6_0',"
            line += "\n    '" + row.find("./Parameter[@Name='ShaderOp.Text']").text.strip().replace("\n", "\\n\\\n") + "'"
            line += ")"
            f.write(line + "\n")
   f.close()

if __name__ == "__main__":
   extract()