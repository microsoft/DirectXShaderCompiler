ParameterTypes = [
    ('Warp.Version', 'unsigned int', False),
    ('Validation.Type', 'String', False),
    ('Validation.Tolerance', 'double', False),
    ('Validation.Input1', 'String', True),
    ('Validation.Input2', 'String', True),
    ('Validation.Input3', 'String', True),
    ('Validation.Input4', 'String', True),
    ('Validation.Expected1', 'String', True),
    ('Validation.Expected2', 'String', True),
    ('Validation.NumInput', 'unsigned int', False),
    ('ShaderOp.Description', 'String', False),
    ('ShaderOp.EntryPoint', 'String', False),
    ('ShaderOp.Name', 'String', False),
    ('ShaderOp.Target', 'String', False),
    ('ShaderOp.Text', 'String', False),
]

class Validation(object):
    """
        Validation object for single instruciton.
    """
    def __init__(self, datatype, check_method, tolerance, inputs, outputs):
        """
            inputs - 2d array of inputs
            outputs - 2d array of expected outputs from inputs
        """
        self.datatype = datatype
        self.check_method = check_method
        self.tolerance = tolerance
        self.inputs = inputs
        self.outputs = outputs


class Instruction(object):
    """
        A single instruction for execution test
    """
    def __init__(self, instruction, num_inputs, num_outputs, validations):
        self.instruction = instruction
        self.num_inputs = num_inputs
        self.num_outputs = num_outputs
        self.validations = validations




