#!/usr/bin/env python3

"""Verify the supplied proof schedules without searching for new plans."""

from pathlib import Path
import subprocess
import sys

def main() -> int:
    root = Path(__file__).resolve().parent.parent
    check = [sys.executable, str(root / 'pinwheel_scheduling_packing_prover.py'),
             '--quality-check', '--max-steps', '0']
    cases = [(dimension, False) for dimension in range(1, 9)] + [(9, True)]
    failures = 0
    for dimension, potential in cases:
        suffix = '_potential' if potential else ''
        proof = root / 'packing_plans' / f'proof_{dimension}{suffix}.txt'
        command = check + ['--dimension', str(dimension), '--plans', str(proof)]
        if potential:
            command.append('--potential')
        print(f'Checking {proof.name}', flush=True)
        result = subprocess.run(command, cwd=root)
        failures += result.returncode != 0
    print(f'{len(cases) - failures}/{len(cases)} proof checks passed.', flush=True)
    return 1 if failures else 0

if __name__ == '__main__':
    sys.exit(main())
