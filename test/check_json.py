#! /usr/local/bin/python
import json
import sys


def main(filename):
    with open(filename) as f:
        data = json.load(f)

    # print(data)

    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
