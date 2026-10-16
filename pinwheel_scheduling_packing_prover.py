#!/usr/bin/env python3

import argparse
from fractions import Fraction
import itertools
import logging
import lzma
import math
import pathlib
import re
import string
import subprocess
import sys

from pysmt.shortcuts import Portfolio, Solver, Symbol, REAL, And, Not, Or, LE, GE, LT, GT, Real, Equals, Plus, Times, Div, is_sat, get_model, to_smtlib
from pysmt.logics import QF_LRA

DEFAULT_LIBRARIES = ['packing_plans/combined.txt']
DEFAULT_TOTAL_DENSITY = '5/6'
DEFAULT_MIN_DENSITY = Fraction(0,1)
DEFAULT_MAX_DENSITY = Fraction(1,1)
DEFAULT_MAX_STEPS   = 10**9
CPP_BUILD = pathlib.Path(__file__).resolve().parent / 'cpp_build'
PACKING_SCHEDULE_FIND = CPP_BUILD / 'packing_schedule_find'
PACKING_SCHEDULE_QUALITY = CPP_BUILD / 'packing_schedule_quality'

def path_reader(path, encoding='utf-8'):
    path = pathlib.Path(path)
    opener = open
    if path.suffix == ".lzma":
        opener = lzma.open
    with opener(path, "rt", encoding=encoding) as f:
        for line in f:
            line = line.strip()
            if line and not line[0] == '#':
                yield line

class UpperBound:
    def __init__(self, description):
        self.strict = True
        assert len(description) > 0 and description[0] == '<'
        description = description[1:]
        if len(description) > 0 and description[0] == '=':
            self.strict = False
            description = description[1:]
        self.value = Fraction(description)
    def __repr__(self):
        result = '<'
        if not self.strict:
            result += '='
        result += str(self.value)
        return result
    def formula(self, symbol):
        d = Div(Real(self.value.numerator), Real(self.value.denominator))
        if self.strict:
            return LT(symbol, d)
        else:
            return LE(symbol, d)

class Plan:
    def __init__(self, description, quality_check=False):
        desc = re.split(r'[ _,;]',description.strip())
        self.description = desc[0]
        self.dimension = len(set(self.description))
        if not quality_check and len(desc) == self.dimension + 1:
            self.constraints = [ UpperBound(c) for c in desc[1:] ]
        else:
            analyze = subprocess.run([str(PACKING_SCHEDULE_QUALITY), self.description], stdout=subprocess.PIPE, check=True, text=True).stdout.strip()
            desc = re.split(r'[ _,;]', analyze)
            self.description = desc[0]
            self.constraints = [ UpperBound(c) for c in desc[1:] ]

    def __repr__(self):
        return f'{self.description}: {" ".join(map(str, self.constraints))}'

    def formula(self, symbols):
        if self.dimension != len(symbols):
            return None
        bounds = list()
        for symbol, bound in zip(symbols, self.constraints):
            bounds.append(bound.formula(symbol))
        return Not(And(bounds))

class PointFinder:

    def __init__(self, *, dimension, total_density, min_density, max_density, plans=[], subproblem=[], potential=False):
        self.dimension = int(dimension)
        self.total_density = Fraction(total_density)
        self.potential = bool(potential)
        self.min_density = Fraction(min_density)
        self.max_density = Fraction(max_density)
        self.symbols = tuple([ Symbol(a, REAL) for a in string.ascii_lowercase[:self.dimension] ])
        self.plans = list(plans)
        subproblem = subproblem[:self.dimension]
        subproblem += [(0,1)]*(self.dimension-len(subproblem))
        self.subproblem = tuple([ (Fraction(x), Fraction(y)) for x,y in subproblem ])

    @property
    def domain_formula(self):
        formula = []
        min=Div(Real(self.min_density.numerator), Real(self.min_density.denominator))
        max=Div(Real(self.max_density.numerator), Real(self.max_density.denominator))
        for s,sub in zip(self.symbols, self.subproblem):
            formula.append(LE(Real(0), s))  # s_i >= 0
            formula.append(LT(min, s))  # s_i > min
            formula.append(LE(s, max))  # s_i <= max
            sub_min=Div(Real(sub[0].numerator), Real(sub[0].denominator))
            sub_max=Div(Real(sub[1].numerator), Real(sub[1].denominator))
            formula.append(LE(sub_min, s))  # s_i > min
            formula.append(LT(s, sub_max))  # s_i <= max

        tgt = Div(Real(self.total_density.numerator), Real(self.total_density.denominator))
        if self.potential:
            formula.append(GE(tgt, Plus(self.symbols[:-1])))
        else:
            formula.append(GE(tgt, Plus(self.symbols)))
        return And(*formula)

    @property
    def symmetry_formula(self):
        formula = []
        for s, ps in zip(self.symbols[1:], self.symbols):
            formula.append(LE(s, ps))       # s_{i+1} <= s_i
        return And(*formula)

    @property
    def plans_formula(self):
        formula = []
        for plan in self.plans:
            subformula = plan.formula(self.symbols)
            if subformula:
                formula.append(subformula)
        return And(*formula)

    @property
    def formula(self):
        return And(self.domain_formula, self.symmetry_formula, self.plans_formula)

    def __call__(self):
        with Solver(name='z3', logic=QF_LRA) as solver:
            solver.add_assertion(self.formula)
            return self.solve(solver)

    def solve(self, solver):
        """Find an uncovered point using an already initialized solver."""
        if solver.solve():
            model = solver.get_model()
            return [Fraction(str(model.get_value(symbol))) for symbol in self.symbols]
        return None

class Prover:

    def __init__(self, *, dimension, total_density, min_density, max_density, potential=False, quality_check=False, plans=[], libraries=[], max_steps=None, partial_proof=None):
        self.dimension = dimension
        self.total_density = total_density
        self.min_density = min_density
        self.max_density = max_density
        self.potential = potential
        self.quality_check = quality_check
        self.plans = plans
        self.libraries = libraries
        self.max_steps = max_steps
        self.partial_proof = partial_proof

    def find_plan(self, point):
        args = [str(PACKING_SCHEDULE_FIND)]
        for library in self.libraries:
            args += ['--library', str(library)]
        for frac in point:
            if frac.numerator > 0:
                args += [f'{frac}',]
        logging.debug('Calling Planner: ' + ' '.join(args))
        result = subprocess.run(args, stdout=subprocess.PIPE, ).stdout.decode('utf-8')
        if result:
            return result
        else:
            return None

    def __call__(self):
        finder = PointFinder(dimension=self.dimension, total_density=self.total_density, min_density=self.min_density, max_density=self.max_density, potential=self.potential, plans=self.plans)
        with Solver(name='z3', logic=QF_LRA, incremental=True) as solver:
            solver.add_assertion(finder.formula)
            return self._prove(finder, solver)

    def _prove(self, finder, solver):
        for iter in itertools.count():
            point = finder.solve(solver)
            if point is None:
                print(f'Theorem is proved: Every packing instance with {self.dimension} real periods and {"potential" if self.potential else "density"} at most {self.total_density} is schedulable')
                return True
            else:
                p = ' , '.join([f'{frac}' for frac in point if frac.numerator])
                if self.max_steps is not None and iter >= self.max_steps:
                    print(f'The maximum number of proof steps reached; uncovered point: ( {p} )')
                    return False
                plan = self.find_plan(point)
                if not plan:
                    print(f' * [{iter + 1}] Point ( {p} ) is not dominated by the current set of plans: Failed to prove theorem!')
                    print(f'Look for a plan ( {p} )')
                    return False
                sch = re.split(r'[ ,;]', plan.strip())[0]
                print(f' * [{iter + 1}] Point ( {p} ) is not dominated by the current set of plans: Adding plan #  {sch}  # to the dominating set')
                new_plan = Plan(plan, self.quality_check)
                constraint = new_plan.formula(finder.symbols)
                if constraint is not None:
                    solver.add_assertion(constraint)
                self.plans.append(new_plan)
                if self.partial_proof:
                    self.partial_proof.parent.mkdir(parents=True, exist_ok=True)
                    with self.partial_proof.open('a') as proof_file:
                        proof_file.write(f'{plan.strip()}\n')

def main():
    parser = argparse.ArgumentParser(description='Automatic Prover for pinwheel scheduling with real weights and bounded number of periods.')
    parser.add_argument('--debug', action='store_true', default=False, help='Print debug information.')
    parser.add_argument('--dimension', type=int, default=2, help='Number of real periods.')
    parser.add_argument('--total-density', type=str, default=DEFAULT_TOTAL_DENSITY, help='The total sum of densities of all tasks. Given as a fraction. Usually it is 5/6.')
    parser.add_argument('--min-density', type=Fraction, default=DEFAULT_MIN_DENSITY, help='The minimum density of a single task. Given as a fraction. Usually it is 0.')
    parser.add_argument('--max-density', type=Fraction, default=DEFAULT_MAX_DENSITY, help='The maximum density of a single task. Given as a fraction. Usually it is 1.')
    parser.add_argument('--potential', action='store_true', default=False, help='Run potential variant, where the potential sum - min is considered.')
    parser.add_argument('--quality-check', action='store_true', default=False, help='Run additional check of calculated plan qualities.')
    parser.add_argument('--plan', type=str, action='append', default=None, help='Add a particular cyclic schedule to the proof. For example ABAC.')
    parser.add_argument('--plans', type=pathlib.Path, action='append', default=None, help='Add all plans from a file to the proof.')
    parser.add_argument('--library', type=pathlib.Path, action='append', default=None, help='Add a file with plans that will be automatically added to the proof when needed.')
    parser.add_argument('--max-steps', type=int, default=DEFAULT_MAX_STEPS, help='Maximum number of new plans to find; 0 only checks the supplied plans.')
    parser.add_argument('--proof', type=pathlib.Path, default=None, help='Output set of proving shedules to a file.')
    parser.add_argument('--partial-proof', type=pathlib.Path, default=None, help='Output set of partial proving shedules to a file.')
    plans = []
    libraries = DEFAULT_LIBRARIES
    args = parser.parse_args()
    missing = [path for path in (PACKING_SCHEDULE_FIND, PACKING_SCHEDULE_QUALITY) if not path.is_file()]
    if missing:
        parser.exit(1, 'Missing C++ binaries: ' + ', '.join(map(str, missing)) + '\nBuild them with: uv run cpp-build\n')
    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    total_density = sum([ Fraction(part) for part in args.total_density.split('+') ], Fraction(0))
    logging.debug(f'Working in dimension {args.dimension}.')
    logging.debug(f'Bounding {"potential" if args.potential else "density"} by {total_density=}.')
    logging.debug(f'Bounding each density in ({args.min_density}, {args.max_density}].')
    if args.plan:
        for plan in args.plan:
            plans.append(Plan(plan, quality_check=args.quality_check))
    if args.plans:
        for path in args.plans:
            for line in path_reader(path):
                plans.append(Plan(line, quality_check=args.quality_check))
    if args.library:
        libraries = []
        for path in args.library:
            libraries.append(str(path))
    old_plans = set([plan.description for plan in plans])
    prover = Prover(dimension=args.dimension, total_density=total_density, min_density=args.min_density, max_density=args.max_density, potential=args.potential, quality_check=args.quality_check, plans=plans, libraries=libraries, max_steps=args.max_steps, partial_proof=args.partial_proof)
    result = prover()
    if result and args.proof:
        args.proof.parent.mkdir(parents=True, exist_ok=True)
        new_plans = set()
        for plan in prover.plans:
            new_plans.add(plan.description)
        for plan in old_plans:
            new_plans.discard(plan)
        with args.proof.open('w') as proof_file:
            for plan in new_plans:
                proof_file.write(f'{plan}\n')
    return 0 if result else 1

if __name__ == '__main__':
    sys.exit(main())
