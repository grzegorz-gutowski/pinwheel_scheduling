#!/usr/bin/env python3

import argparse
from fractions import Fraction
import itertools
import logging
import lzma
import pathlib
import re
import subprocess
import sys

from pysmt.shortcuts import Solver, Symbol, REAL, And, Not, LE, GE, LT, GT, Real, Plus, Div
from pysmt.logics import QF_LRA

DEFAULT_LIBRARIES = ['packing_plans/combined.txt']
DEFAULT_THRESHOLD = '5/6'
DEFAULT_MIN_DENSITY = Fraction(0,1)
DEFAULT_MAX_DENSITY = Fraction(1,1)
DEFAULT_MAX_STEPS   = 10**9
CPP_BUILD = pathlib.Path(__file__).resolve().parent / 'cpp_build'
PACKING_SCHEDULE_FIND = CPP_BUILD / 'packing_schedule_find'
PACKING_SCHEDULE_QUALITY = CPP_BUILD / 'packing_schedule_quality'


def path_reader(path, encoding='utf-8'):
    path = pathlib.Path(path)
    opener = open
    if path.suffix == '.lzma':
        opener = lzma.open
    with opener(path, 'rt', encoding=encoding) as f:
        for line in f:
            line = line.strip()
            if line and not line[0] == '#':
                yield line

class UpperBound:
    def __init__(self, description):
        self.strict = True
        if not description.startswith('<'):
            raise ValueError('Expected a schedule bound starting with < or <=')
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
    def formula(self, density):
        upper = Div(Real(self.value.numerator), Real(self.value.denominator))
        if self.strict:
            return LT(density, upper)
        else:
            return LE(density, upper)

class PeriodicSchedule:
    def __init__(self, description, recompute_box=False):
        desc = re.split(r'[\s_,;]+', description.strip())
        self.word = desc[0]
        if not re.fullmatch(r'[A-Z]+', self.word):
            raise ValueError('A schedule must be a nonempty word over A–Z')
        self.dimension = len(set(self.word))
        if not recompute_box and len(desc) == self.dimension + 1:
            self.density_bounds = [ UpperBound(c) for c in desc[1:] ]
        else:
            analyze = subprocess.run([str(PACKING_SCHEDULE_QUALITY), self.word], stdout=subprocess.PIPE, check=True, text=True).stdout.strip()
            desc = re.split(r'[\s_,;]+', analyze)
            if (not re.fullmatch(r'[A-Z]+', desc[0])
                    or len(set(desc[0])) != self.dimension
                    or len(desc) != self.dimension + 1):
                raise ValueError('Dominance-box computation returned an invalid schedule or bound count')
            self.word = desc[0]
            self.density_bounds = [ UpperBound(c) for c in desc[1:] ]

    def __repr__(self):
        density_bounds = ' '.join(map(str, self.density_bounds))
        return f'{self.word}: {density_bounds}'

    def outside_box_formula(self, densities):
        if self.dimension != len(densities):
            return None
        bounds = list()
        for density, bound in zip(densities, self.density_bounds):
            bounds.append(bound.formula(density))
        return Not(And(bounds))

class UndominatedVectorFinder:

    def __init__(self, *, dimension, threshold, min_density, max_density, schedules=(), potential=False, bounds=()):
        self.dimension = dimension
        self.threshold = Fraction(threshold)
        self.potential = bool(potential)
        self.min_density = Fraction(min_density)
        self.max_density = Fraction(max_density)
        self.bounds = tuple(bounds)
        self.densities = tuple(Symbol(f'd_{i}', REAL) for i in range(1, self.dimension + 1))
        self.schedules = list(schedules)

    @property
    def domain_formula(self):
        formula = []
        lower = Div(Real(self.min_density.numerator), Real(self.min_density.denominator))
        upper = Div(Real(self.max_density.numerator), Real(self.max_density.denominator))
        for d_i in self.densities:
            formula.append(LE(Real(0), d_i))
            formula.append(LT(lower, d_i))
            formula.append(LE(d_i, upper))
        threshold = Div(Real(self.threshold.numerator), Real(self.threshold.denominator))
        if self.potential:
            formula.append(GE(threshold, Plus((Real(0),) + self.densities[:-1])))
        else:
            formula.append(GE(threshold, Plus(self.densities)))
        operators = {'<': LT, '<=': LE, '>': GT, '>=': GE}
        for index, operator, value in self.bounds:
            formula.append(operators[operator](self.densities[index-1], Real(value)))
        return And(*formula)

    @property
    def density_order_formula(self):
        formula = []
        for d_i, d_next in zip(self.densities, self.densities[1:]):
            formula.append(LE(d_next, d_i))  # d_{i+1} <= d_i
        return And(*formula)

    @property
    def undominated_formula(self):
        formula = []
        for schedule in self.schedules:
            subformula = schedule.outside_box_formula(self.densities)
            if subformula:
                formula.append(subformula)
        return And(*formula)

    @property
    def formula(self):
        return And(self.domain_formula, self.density_order_formula, self.undominated_formula)

    def __call__(self):
        with Solver(name='z3', logic=QF_LRA) as solver:
            solver.add_assertion(self.formula)
            return self.solve(solver)

    def solve(self, solver):
        if solver.solve():
            model = solver.get_model()
            return [Fraction(str(model.get_value(d_i))) for d_i in self.densities]
        return None

class Prover:

    def __init__(self, *, dimension, threshold, min_density, max_density, potential=False, recompute_box=False, schedules=(), libraries=(), max_steps=None, partial_certificate=None, bounds=()):
        self.dimension = dimension
        self.threshold = threshold
        self.min_density = min_density
        self.max_density = max_density
        self.potential = potential
        self.recompute_box = recompute_box
        self.schedules = list(schedules)
        self.libraries = list(libraries)
        self.max_steps = max_steps
        self.partial_certificate = partial_certificate
        self.bounds = bounds

    def find_dominating_schedule(self, density_vector):
        args = [str(PACKING_SCHEDULE_FIND)]
        for library in self.libraries:
            args += ['--library', str(library)]
        for frac in density_vector:
            if frac.numerator > 0:
                args += [f'{frac}',]
        logging.debug('Calling schedule constructor: ' + ' '.join(args))
        result = subprocess.run(args, stdout=subprocess.PIPE, ).stdout.decode('utf-8')
        if result:
            return result
        else:
            return None

    def __call__(self):
        finder = UndominatedVectorFinder(dimension=self.dimension, threshold=self.threshold, min_density=self.min_density, max_density=self.max_density, potential=self.potential, schedules=self.schedules, bounds=self.bounds)
        with Solver(name='z3', logic=QF_LRA, incremental=True) as solver:
            solver.add_assertion(finder.formula)
            return self._prove(finder, solver)

    def _prove(self, finder, solver):
        for iter in itertools.count():
            density_vector = finder.solve(solver)
            if density_vector is None:
                restrictions = f'; each density in ({self.min_density}, {self.max_density}]'
                restrictions += ''.join(f'; d_{i} {op} {v}' for i, op, v in self.bounds)
                quantity = 'potential' if self.potential else 'density'
                print(f'The supplied words dominate the specified domain: dimension {self.dimension}; {quantity} at most {self.threshold}{restrictions}')
                return True
            else:
                p = ' , '.join([f'{frac}' for frac in density_vector if frac.numerator])
                if self.max_steps is not None and iter >= self.max_steps:
                    print(f'The maximum number of construction steps reached; undominated density vector: ( {p} )')
                    return False
                schedule = self.find_dominating_schedule(density_vector)
                if not schedule:
                    print(f' * [{iter + 1}] Density vector ( {p} ) is not dominated by the current family of words: The supplied words do not dominate the specified domain.')
                    print(f'Look for a dominating schedule for ( {p} )')
                    return False
                sch = re.split(r'[ ,;]', schedule.strip())[0]
                print(f' * [{iter + 1}] Density vector ( {p} ) is not dominated by the current family of words: Adding word #  {sch}  # to the dominating set')
                new_schedule = PeriodicSchedule(schedule, self.recompute_box)
                constraint = new_schedule.outside_box_formula(finder.densities)
                if constraint is not None:
                    solver.add_assertion(constraint)
                self.schedules.append(new_schedule)
                if self.partial_certificate:
                    self.partial_certificate.parent.mkdir(parents=True, exist_ok=True)
                    with self.partial_certificate.open('a') as certificate_file:
                        certificate_file.write(f'{schedule.strip()}\n')

def write_certificate(path, schedules):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w') as certificate_file:
        for word in dict.fromkeys(schedule.word for schedule in schedules):
            certificate_file.write(f'{word}\n')


def coordinate_bounds(bounds, dimension):
    result = []
    for description in bounds:
        match = re.fullmatch(r'\s*d_([0-9]+)\s*(<=|>=|<|>)\s*(.+?)\s*', description)
        if match is None:
            raise ValueError('Expected a density bound such as d_1>=3/20 or d_9<3/40')
        index, operator, value = match.groups()
        index = int(index)
        if not 1 <= index <= dimension:
            raise ValueError(f'Coordinate must be between 1 and {dimension}')
        if operator not in ('<', '<=', '>', '>='):
            raise ValueError('Coordinate operator must be <, <=, >, or >=')
        result.append((index, operator, Fraction(value)))
    return tuple(result)

def main():
    parser = argparse.ArgumentParser(allow_abbrev=False, description='Construct periodic schedules and verify domination for real-period pinwheel scheduling in bounded dimension.')
    parser.add_argument('--debug', action='store_true', help='Print debug information.')
    parser.add_argument('--dimension', type=int, default=2, help='Dimension of the instance (number of tasks).')
    thresholds = parser.add_mutually_exclusive_group()
    thresholds.add_argument('--density-bound', help='Inclusive bound on instance density D(A), as a fraction (default: 5/6).')
    thresholds.add_argument('--potential-bound', help='Inclusive bound on instance potential P(A), the sum of all but the smallest task density.')
    parser.add_argument('--min-density', type=Fraction, default=DEFAULT_MIN_DENSITY, help='Strict lower bound on each task density, as a fraction (default: 0).')
    parser.add_argument('--max-density', type=Fraction, default=DEFAULT_MAX_DENSITY, help='Inclusive upper bound on each task density, as a fraction (default: 1).')
    parser.add_argument('--bound', action='append', default=[], help='Additional bound on ordered densities, e.g. d_1>=3/20. Repeat to conjoin bounds; quote shell operators.')
    parser.add_argument('--recompute-boxes', dest='recompute_box', action='store_true', help='Recompute dominance boxes from the supplied words.')
    parser.add_argument('--word', action='append', help='Add a word representing a periodic schedule, for example ABAC.')
    parser.add_argument('--certificate', type=pathlib.Path, action='append', help='Read words and optional dominance-box bounds from a certificate file.')
    parser.add_argument('--library', type=pathlib.Path, action='append', help='Read a library of words used to construct dominating schedules.')
    parser.add_argument('--max-steps', type=int, default=DEFAULT_MAX_STEPS, help='Maximum number of new dominating words to find; 0 only checks the supplied words.')
    parser.add_argument('--write-certificate', type=pathlib.Path, help='On success, write all words used, including supplied words.')
    parser.add_argument('--partial-certificate', type=pathlib.Path, help='Append newly found words and their dominance-box bounds to a file.')
    args = parser.parse_args()
    potential = args.potential_bound is not None
    threshold_text = args.potential_bound if potential else args.density_bound
    if threshold_text is None:
        threshold_text = DEFAULT_THRESHOLD
    try:
        bounds = coordinate_bounds(args.bound, args.dimension)
        threshold = sum((Fraction(part) for part in threshold_text.split('+')), Fraction(0))
        if not 0 <= args.min_density < args.max_density:
            raise ValueError('Densities must satisfy 0 <= min-density < max-density')
    except (ValueError, ZeroDivisionError) as error:
        parser.error(str(error))
    missing = [path for path in (PACKING_SCHEDULE_FIND, PACKING_SCHEDULE_QUALITY) if not path.is_file()]
    if missing:
        parser.exit(1, 'Missing C++ binaries: ' + ', '.join(map(str, missing)) + '\nBuild them with: uv run cpp-build\n')
    logging.basicConfig(level=logging.DEBUG if args.debug else logging.INFO)
    logging.debug(f'Working in dimension {args.dimension}.')
    quantity = 'potential' if potential else 'density'
    logging.debug(f'Bounding {quantity} by {threshold=}.')
    logging.debug(f'Bounding each density in ({args.min_density}, {args.max_density}].')
    schedules = []
    for word in args.word or ():
        schedules.append(PeriodicSchedule(word, recompute_box=args.recompute_box))
    for path in args.certificate or ():
        for line in path_reader(path):
            schedules.append(PeriodicSchedule(line, recompute_box=args.recompute_box))
    libraries = [str(path) for path in args.library] if args.library else DEFAULT_LIBRARIES
    prover = Prover(dimension=args.dimension, threshold=threshold, min_density=args.min_density,
                    max_density=args.max_density, potential=potential, recompute_box=args.recompute_box,
                    schedules=schedules, libraries=libraries, max_steps=args.max_steps,
                    partial_certificate=args.partial_certificate, bounds=bounds)
    result = prover()
    if result and args.write_certificate:
        write_certificate(args.write_certificate, prover.schedules)
    return 0 if result else 1

if __name__ == '__main__':
    sys.exit(main())
