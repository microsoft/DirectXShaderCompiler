"""ExtractIRForPassTest.py - extract IR just before selected pass would be run

This script automates some operations to make it easier to write IR tests:
  1. Gets the pass list for an HLSL compilation using -Odump
  2. Compiles HLSL with -fcgl and outputs to intermediate IR
  3. Collects list of passes before the desired pass and adds
     -hlsl-passes-pause to write correct metadata
  4. Invokes dxopt to run passes on -fcgl output and write bitcode result
  5. Disassembles bitcode to .ll file for use as a test
  6. Inserts RUN line with -hlsl-passes-resume and desired pass

Usage:
  ExtractIRForPassTest.py [<number>] <-desired-pass> <hlsl-file> <output-file>
                          <compilation options for HLSL>

  <number>        - if a pass appears multiple times in the pass list,
                    this 1-based number selects the instance on which to stop
  <-desired-pass> - name of pass to stop at for testing (with leading '-')
                    Note: stopping at per-function prepass not supported
  <hlsl-file>     - name of the input HLSL file to compile
  <output-file>   - name of the output file
  <compilation options for HLSL>
                  - set of compilation options needed to compile the HLSL file
"""

import os
import sys
import subprocess
import tempfile

def Usage():
  print(__doc__)
  exit(3)

class Options(object):
  def __init__(self):
    self.instance = 1

def ParseArgs(args):
  opts = Options()

  try:
    opts.instance = int(args[0])
    args = args[1:]
  except ValueError:
    pass

  # At this point, need at least:
  # <desired-pass> <hlsl-file> <output-file>
  # and at least the -T compilation option.
  if len(args) < 4:
    Usage()

  opts.desired_pass = args[0]
  opts.hlsl_file = args[1]
  opts.fcgl_file = opts.hlsl_file + '.fcgl.ll'
  opts.output_file = args[2]
  opts.compilation_options = args[3:]
  return opts

def SplitAtPass(passes, pass_name, instance = 1):
  before = []
  fn_passes = True
  count = 0
  after = None
  for line in passes:
    line = line.strip()
    if not line or line.startswith('#'):
      continue
    if line == '-opt-mod-passes':
      fn_passes = False
    if after:
      after.append(line)
      continue
    if not fn_passes:
      if line == pass_name:
        count += 1
        if count >= instance:
          after = [line]
          continue
    before.append(line)
  return before, after

def GetTempFilename(*args, **kwargs):
  "Get temp filename and close the file handle for use by others"
  fd, name = tempfile.mkstemp(*args, **kwargs)
  os.close(fd)
  return name

def main(opts):
  # 1. Gets the pass list for an HLSL compilation using -Odump
  cmd = ['dxc', '/Odump', opts.hlsl_file] + opts.compilation_options
  # print(cmd)
  all_passes = subprocess.check_output(cmd, text=True)
  all_passes = all_passes.splitlines()

  # 2. Compiles HLSL with -fcgl and outputs to intermediate IR
  fcgl_file = GetTempFilename('.ll')
  cmd = (['dxc', '-fcgl', '-Fc', fcgl_file, opts.hlsl_file]
         + opts.compilation_options)
  # print(cmd)
  subprocess.check_call(cmd)

  # 3. Collects list of passes before the desired pass and adds
  #    -hlsl-passes-pause to write correct metadata
  passes_before, passes_after = SplitAtPass(
    all_passes, opts.desired_pass, opts.instance)
  print('\nPasses before: {}\n\nRemaining passes: {}'
          .format(' '.join(passes_before), ' '.join(passes_after)))
  passes_before.append('-hlsl-passes-pause')

  # 4. Invokes dxopt to run passes on -fcgl output and write bitcode result
  bitcode_file = GetTempFilename('.bc')
  cmd = ['dxopt', '-o=' + bitcode_file, fcgl_file] + passes_before
  # print(cmd)
  subprocess.check_call(cmd)

  # 5. Disassembles bitcode to .ll file for use as a test
  temp_out = GetTempFilename('.ll')
  cmd = ['dxc', '/dumpbin', '-Fc', temp_out, bitcode_file]
  # print(cmd)
  subprocess.check_call(cmd)

  # 6. Inserts RUN line with -hlsl-passes-resume and desired pass
  with open(opts.output_file, 'wt') as f:
    f.write('; RUN: %opt %s -hlsl-passes-resume {} -S | FileCheck %s\n\n'
              .format(opts.desired_pass))
    with open(temp_out, 'rt') as f_in:
      f.write(f_in.read())

  # Clean up temp files
  os.unlink(fcgl_file)
  os.unlink(bitcode_file)
  os.unlink(temp_out)

if __name__=='__main__':
  opts = ParseArgs(sys.argv[1:])
  main(opts)
  print('\nSuccess!  See output file:\n{}'.format(opts.output_file))
