#!/usr/bin/env python3
"""Compatibility entry point for the complete original-object coverage pack."""
import argparse
from pathlib import Path
from stage_coverage import build
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path)
    build(parser.parse_args().game)
