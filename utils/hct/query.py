# user defined function
def query(insts, inst):
    # example
    """
    if inst.ret_type == "v":
        return true
    return false
    """

    # example
    """
    if inst.fn_attr == "" and "Quad" in inst.name:
        found = false
        for other_inst in insts:
            if not (other_inst == inst) and "Quad" in other_inst.name and other_inst.fn_attr != ""
                found = true
                break
        return found
        
    return false
    """

    # example
    if inst.ret_type == "i32" and len(inst.args) > 2:
        return True
    return False

