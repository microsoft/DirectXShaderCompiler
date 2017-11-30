# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.
###############################################################################
# This file contains driver test information for DXIL operations              #
###############################################################################

from hctdb import *
g_db_dxil = None
def get_db_dxil():
    global g_db_dxil
    if g_db_dxil is None:
        g_db_dxil = db_dxil()
    return g_db_dxil

# name to instruction pair
g_instructions = { }

# This class represents a possible test case for drivers
class test_case(object):
    def __init__(self):    
        self.validation_type = ""
        self.validation_tolerance = 0
        self.inputs = []
        self.outputs = []
        self.shader_text = ""
        self.shader_target = ""
        self.test_enabled = False

# This class represents a test case for each instruction
class dxil_inst_test_case(test_case):
    def __init__(self, inst):
        self.inst = inst

def update_test_case(name, validation_type, validation_tolerance, inputs, outputs, shader_target, shader_text):
    g_instructions[name].inputs = inputs
    g_instructions[name].outputs = outputs
    g_instructions[name].validation_type = validation_type
    g_instructions[name].validation_tolerance = validation_tolerance
    g_instructions[name].shader_target = shader_target
    g_instructions[name].shader_text = shader_text
    g_instructions[name].test_enabled = True

# This is a collection of test case for execution tests
def update_test_cases():
   nan = float('nan')
   p_inf = float('inf')
   n_inf = float('-inf')
   p_denorm = float('1e-38')
   n_denorm = float('-1e-38')
   update_test_case("Sin", "absolute", 0.0008,
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -314.16, 314.16],
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -0.0007346401, 0.0007346401],
                    "cs_6_0", "\
                    struct SUnaryFPOp {\
                        float input;\
                        float output;\
                    };\
                    RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
                    [numthreads(8,8,1)]\
                    [RootSignature(\"RootFlags(0), UAV(u0)\")]\
                    void main(uint GI : SV_GroupIndex) {\
                        SUnaryFPOp l = g_buf[GI];\
                        l.output = sin(l.input);\
                        g_buf[GI] = l;\
                    }")
   update_test_case("Cos", "absolute", 0.0008,
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, -314.16, 314.16],
                    [nan, n_inf, n_denorm, -0, 0, p_denorm, p_inf, 0.99999973015, 0.99999973015],
                    "cs_6_0", 
                    "struct SUnaryFPOp {\
                    float input;\
                    float output;\
                    };\
                    RWStructuredBuffer<SUnaryFPOp> g_buf : register(u0);\
                    [numthreads(8,8,1)]\
                    [RootSignature(\"RootFlags(0), UAV(u0)\")]\
                    void main(uint GI : SV_GroupIndex) {\
                        SUnaryFPOp l = g_buf[GI];\
                        l.output = cos(l.input);\
                        g_buf[GI] = l;\
                    }")
    update_test_case("FMin", "absolute", 0,
        [(n_inf, n_inf), (n_inf, p_inf), (n_inf, 1.0), (n_inf, nan), (p_inf, n_inf), (p_inf, p_inf), (p_inf, 1.0), (p_inf, nan), (nan, n_inf), (nan, p_inf), (nan, 1.0), (nan, nan),(1.0, n_inf), (1.0, p_inf), (-1.0, 1.0), (-1.0, nan), (1.0, -1.0)],
        [n_inf, n_inf, n_inf, n_inf, n_inf, p_inf, 1.0, p_inf, -p_inf, p_inf, 1.0, nan, n_inf, 1.0, -1.0, -1.0, -1.0], 
                "cs_6_0", 
                "struct SBinaryFPOp {\
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
                };")
    update_test_case("FAdd", "ulp", 1, 
    [],
    [],
                "cs_6_0", 
                "struct SBinaryFPOp {\
                    float input1;\
                    float input2;\
                    float output1;\
                };\
                RWStructuredBuffer<SBinaryFPOp> g_buf : register(u0);\
                [numthreads(8,8,1)]\
                void main(uint GI : SV_GroupIndex) {\
                    SBinaryFPOp l = g_buf[GI];\
                    l.output1 = l.input1 + l.input2;
                    g_buf[GI] = l;\
                };")
    )
    

if __name__ == "__main__":
    db = get_db_dxil()
    for i in db.instr:
        g_instructions[i.name] = dxil_inst_test_case(i)
    update_test_cases()
    print(g_instructions['Sin'].shader_target)
    print(g_instructions['FMin'].shader_text)
