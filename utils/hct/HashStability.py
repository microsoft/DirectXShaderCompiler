
import os
import subprocess
import platform
import filecmp
import argparse
import shutil

IS_WINDOWS = platform.system() == 'Windows'

# Don't use close_fds on Windows.
USE_CLOSE_FDS = not IS_WINDOWS


def extract_hash(dxa_path, dx_container, working_dir, empty_env):
    # extract hash using dxa
    hash_file = f"{dx_container}.hash"
    args = [dxa_path, "-extractpart", "HASH",
            dx_container, "-o", hash_file]

    proc = subprocess.Popen(args, cwd=working_dir,
                            env=empty_env,
                            executable=dxa_path,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    stdout, stderr = proc.communicate()
    res = proc.wait()
    if res != 0:
        print(f"extract hash from {dx_container} failed {args}, stdout:{stdout}, stderr:{stderr}")
        # extract hash failed, return fail.
        return None
    return hash_file


def normal_compile(args, output_file, working_dir, empty_env):
    normal_args = args
    normal_args.append("-Qstrip_reflect")
    normal_args.append("-Zsb")
    normal_args.append("-Fo")
    normal_args.append(output_file)
    proc = subprocess.Popen(normal_args, cwd=working_dir,
                            env=empty_env,
                            # don't writ output to stdout
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    stdout, stderr = proc.communicate()
    res = proc.wait()
    if res != 0:
        print(f"normal compile failed {args}, stdout:{stdout}, stderr:{stderr}")
    return res

def debug_compile(args, output_file, working_dir, empty_env):
    debug_args = args
    debug_args.append("-Zi")
    debug_args.append("-Qstrip_reflect")
    debug_args.append("-Zsb")
    debug_args.append("-Fo")
    debug_args.append(output_file)

    proc = subprocess.Popen(debug_args, cwd=working_dir,
                            env=empty_env,
                            # don't writ output to stdout
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    stdout, stderr = proc.communicate()
    res = proc.wait()
    if res != 0:
        print(f"debug compile failed {args}, stdout:{stdout}, stderr:{stderr}")
    return res


def run_hash_stablity_test(args, dxc_path, dxa_path, test_name, working_dir):
    empty_env = os.environ.copy()
    # clear PATH to make sure dxil.dll are not found.
    empty_env["PATH"] = ""
    args[0] = dxc_path

    # run normal compile
    normal_out = os.path.join(working_dir, 'Output', test_name+'.normal.out')
    res = normal_compile(args, normal_out, working_dir, empty_env)
    if res != 0:
        # strip_reflect failed, return fail.
        return True, "normal compile failed, assume this is an expected failure testing shader."

    normal_hash = extract_hash(dxa_path, normal_out, working_dir, empty_env)
    if normal_hash is None:
        return False, "Fail to get hash for normal compilation."

    # run debug compilation
    debug_out = os.path.join(working_dir, 'Output', test_name+'.dbg.out')
    res = debug_compile(args, debug_out, working_dir, empty_env)
    if res != 0:
        # Zi failed, return fail.
        return False, "debug compilation failed."

    debug_hash = extract_hash(dxa_path, debug_out, working_dir, empty_env)
    if debug_hash is None:
        return False, "Fail to get hash for debug compilation."

    # compare normal_hash and debug_hash.
    if filecmp.cmp(normal_hash, debug_hash):
        # hash match, return pass.
        return True, "Hash match."
    else:
        # hash mismatch
        return False, "Hash mismatch."

################################################
################################################
# For running from the command-line

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Run hash stability test')
    parser.add_argument('-a','--argument', help='origin command line options to run dxc like \"dxc -ECSMain -Tcs_6_0 D:\\test.hlsl\"', required=True)
    parser.add_argument('-p','--path', help='path to find dxc and dxa', required=True)
    args = vars(parser.parse_args())
    dxc_args = args['argument'].split(' ')
    dxc_args[0] = "%dxc"
    bin_dir = args['path']
    # get dxc and dxa path when running from command line
    dxc_path = os.path.join(bin_dir, 'dxc.exe')
    dxa_path = os.path.join(bin_dir, 'dxa.exe')
    working_dir = os.getcwd()
    tmp_path = os.path.join(working_dir, 'Output')
    # create tmp_path if it doesn't exist
    if not os.path.exists(tmp_path):
        try:
            os.makedirs(tmp_path)
        except OSError:
            print("Creation of the directory %s for temp output failed" % tmp_path)
            exit(1)

    res, msg = run_hash_stablity_test(dxc_args, dxc_path, dxa_path, "test", working_dir)
    if res:
        print("PASS")
    else:
        print(f"FAIL: {msg}")
    # remove tmp dir
    shutil.rmtree(tmp_path)
