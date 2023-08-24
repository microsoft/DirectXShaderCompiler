
import os
import subprocess
import platform
import filecmp
import argparse
import shutil

IS_WINDOWS = platform.system() == 'Windows'

# Don't use close_fds on Windows.
USE_CLOSE_FDS = not IS_WINDOWS


def extract_hash(dxa_path, dx_container, working_dir):
    # extract hash from dxa_path
    hash_file = f"{dx_container}.hash"
    args = [dxa_path, "-extractpart", "HASH",
            dx_container, "-o", hash_file]

    proc = subprocess.Popen(args, cwd=working_dir,
                            executable=dxa_path,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    proc.communicate()
    res = proc.wait()
    if res != 0:
        print(f"extract hash failed {args}")
        # extract hash failed, return fail.
        return None
    return hash_file


def normal_compile(args, output_file, working_dir):
    normal_args = args
    normal_args.append("-Qstrip_reflect")
    normal_args.append("-Zsb")
    normal_args.append("-Fo")
    normal_args.append(output_file)
    proc = subprocess.Popen(normal_args, cwd=working_dir,
                            # don't writ output to stdout
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    proc.communicate()
    return proc.wait()


def debug_compile(args, output_file, working_dir):
    debug_args = args
    debug_args.append("-Zi")
    debug_args.append("-Qstrip_reflect")
    debug_args.append("-Zsb")
    debug_args.append("-Fo")
    debug_args.append(output_file)

    proc = subprocess.Popen(debug_args, cwd=working_dir,
                            # don't writ output to stdout
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    proc.communicate()
    return proc.wait()


def run_hash_stablity_test(args, dxc_path, dxa_path, test_name, working_dir):
    args[0] = dxc_path
    # run original compile
    proc = subprocess.Popen(args, cwd=working_dir,
                            # don't writ output to stdout
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=USE_CLOSE_FDS)
    proc.communicate()
    res = proc.wait()
    if res != 0:
        # original compilation failed, don't run hash test.
        return True, "Original compilation failed."

    # run normal compile
    normal_out = os.path.join(working_dir, 'Output', test_name+'.normal.out')
    res = normal_compile(args, normal_out, working_dir)
    if res != 0:
        # strip_reflect failed, return fail.
        return False, "Adding Qstrip_reflect failed compilation."

    normal_hash = extract_hash(dxa_path, normal_out, working_dir)
    if normal_hash is None:
        return False, "Fail to get hash for normal compilation."

    # run debug compilation
    debug_out = os.path.join(working_dir, 'Output', test_name+'.dbg.out')
    res = debug_compile(args, debug_out, working_dir)
    if res != 0:
        # strip_reflect failed, return fail.
        return False, "Adding Qstrip_reflect and Zi failed compilation."

    debug_hash = extract_hash(dxa_path, debug_out, working_dir)
    if debug_hash is None:
        return False, "Fail to get hash for normal compilation."

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

def usage():
    print("""HashStability.py - Run hash stability test.

Usage:
HashStability.py <dxc_args> - args to run dxc like " "%dxc -ECSMain -Tcs_6_0 D:\test.hlsl"
    <bin_dir>       - path to dxc.exe and dxa.exe
""")
    return 1

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Description of your program')
    parser.add_argument('-a','--argument', help='origin command line options to run dxc like \"dxc -ECSMain -Tcs_6_0 D:\\test.hlsl\"', required=True)
    parser.add_argument('-p','--path', help='path to find dxc and dxa', required=True)
    args = vars(parser.parse_args())
    dxc_args = args['argument'].split(' ')
    dxc_args[0] = "%dxc"
    bin_dir = args['path']
    dxc_path = os.path.join(bin_dir, 'dxc.exe')
    dxa_path = os.path.join(bin_dir, 'dxa.exe')
    working_dir = os.getcwd()
    tmp_path = os.path.join(working_dir, 'Output')
    print('tmp_path: ' + tmp_path)
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
