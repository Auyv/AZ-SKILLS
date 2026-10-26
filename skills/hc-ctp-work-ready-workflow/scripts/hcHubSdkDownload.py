#!/usr/bin/env python3
"""Backward-compatible entry point; implementation lives in hcCtpWorkReady.py."""
from __future__ import annotations

import sys

from hcCtpWorkReady import main


if __name__ == "__main__":
    sys.exit(main(["download-sdk", *sys.argv[1:]]))
