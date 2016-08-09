#!/usr/bin/python

import os
import re
import sys
from subprocess import Popen, PIPE
from os import path

_DIRNAME = path.dirname(__file__)

HEADER_DIR = 'boost'
HEADERS = [
    'boost/any.hpp'
]


def shell(command):
    print("> {}".format(command))
    proc = Popen(command, shell=True, close_fds=True,
                 stdin=PIPE, stdout=PIPE, stderr=PIPE)
    out, err = proc.communicate()
    if proc.returncode != 0:
        print(out)
        print(err)
        raise Exception("proc.returncode: {}".format(proc.returncode))
    return out


def main():
    os.chdir(_DIRNAME)
    with open('test.cpp', 'w') as f:
        for h in HEADERS:
            f.write('#include "{}"\n'.format(h))
        f.write('int main() {return 0;}\n')
    depends = shell("clang++ -M -I. test.cpp")
    print(depends)
    os.remove('test.cpp')

    used = {}
    for line in depends.split('\n'):
        if not re.search('^ +' + HEADER_DIR + '/', line):
            continue
        if line[-1] == '\\':
            line = line[:-1]
        for h in line.strip().split(' '):
            used[h] = True

    for root, dirs, files in os.walk(HEADER_DIR):
        for file in files:
            file = path.join(root, file)
            if file in used or file in HEADERS:
                print('keep {}'.format(file))
                continue
            os.remove(file)

    # remove empty dirs
    print('removing empty dirs..')
    while True:
        removed = False
        for root, dirs, files in os.walk(HEADER_DIR):
            for dirname in dirs:
                dirname = path.join(root, dirname)
                names = os.listdir(dirname)
                if len(names) == 0:
                    removed = True
                    os.rmdir(dirname)
                    break
            if removed:
                break
        if not removed:
            break

    return 0


if __name__ == '__main__':
    sys.exit(main())
