#!/usr/bin/env python3

'''Verify domination by the supplied certificate words without searching for additional words.'''

from pathlib import Path
import subprocess
import sys
import time

def main() -> int:
    root = Path(__file__).resolve().parent.parent
    check = [sys.executable, str(root / 'pinwheel_scheduling_packing_prover.py'),
             '--recompute-boxes', '--max-steps', '0', '--max-density', '1']
    cases = [(dimension, False) for dimension in range(1, 9)] + [(9, True)]
    failures = 0
    total_time = 0
    total_words = 0
    max_ratio = 0
    for dimension, potential, *bounds in cases:
        prefix = 'P' if potential else 'D'
        filename = f'{prefix}{dimension}.txt'
        certificate = root / 'packing_plans/proof_5_6' / filename
        words = [word.split(',')[0] for word in certificate.read_text().splitlines() if word]
        max_len = max(map(len, words))
        max_ratio = max(max_ratio, max_len / 2**dimension)
        command = check + ['--dimension', str(dimension), '--certificate', str(certificate)]
        command.extend(['--potential-bound' if potential else '--density-bound', '5/6'])
        for bound in bounds:
            command.extend(['--bound', bound])
        domain = f'{prefix}_{dimension}'
        print(f'Checking certificate {certificate.name} for {domain}: {len(words)} words, maximum word length {max_len}', flush=True)
        start = time.monotonic()
        result = subprocess.run(command, cwd=root)
        elapsed = time.monotonic() - start
        failure = ' NOT' if result.returncode != 0 else ''
        print(f'Domination{failure} verified in {elapsed:.2f}s')
        total_time += elapsed
        total_words += len(words)
        failures += result.returncode != 0
    print(f'{len(cases) - failures}/{len(cases)} certificate checks passed with {total_words} words in {total_time:.2f}s. maximum word length / 2**dimension is {max_ratio}', flush=True)
    return 1 if failures else 0

if __name__ == '__main__':
    sys.exit(main())
