#!/usr/bin/env python3
import sys

from az_sdd import main


if __name__ == "__main__":
    sys.exit(main(["rebuild-indexes", *sys.argv[1:]]))
