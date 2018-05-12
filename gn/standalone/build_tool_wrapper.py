#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" Wrapper to invoke compiled build tools from the build system.

This is just a workaround for GN assuming that all external scripts are
python sources. It is used to invoke tools like the protoc compiler.
"""

import argparse
import os
import subprocess
import sys

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--chdir', default=None)
  parser.add_argument('--stamp', default=None)
  parser.add_argument('cmd', nargs=argparse.REMAINDER)
  args = parser.parse_args()

  if args.chdir and not os.path.exists(args.chdir):
    print >> sys.stderr, 'Cannot chdir to %s from %s' % (workdir, os.getcwd())
    return 1

  if not os.path.exists(args.cmd[0]):
    print >> sys.stderr, 'Cannot find ' + args.cmd[0]
    return 1

  abscmd = os.path.abspath(args.cmd[0])
  proc = subprocess.Popen([abscmd] + args.cmd[1:], cwd=args.chdir)
  ret = proc.wait()
  if ret == 0 and args.stamp:
    with open(args.stamp, 'w'):
      os.utime(args.stamp, None)

  return ret

if __name__ == '__main__':
  sys.exit(main())
