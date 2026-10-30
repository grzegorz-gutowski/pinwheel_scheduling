# Pinwheel Scheduling

Tools for constructing periodic schedules and verifying computer-assisted
proofs for real-period pinwheel scheduling. The repository includes schedule
certificates for the density bound 5/6 and the density bound 17/20 with minimum
period at least 4.

## Instances, schedules, and domination

A real-valued **instance** is a sequence of task periods
$A=(a_1,\ldots,a_n)$ with $1\leq a_1\leq\cdots\leq a_n$; its **dimension** is $n$.
A **schedule** assigns one task to each day. It is **valid** for $A$ if, for
every task $i$ and positive integer $\ell$, task $i$ occurs at least $\ell$ times
in every block of $\lceil\ell a_i\rceil$ consecutive days. An instance is
**schedulable** if it has a valid schedule.

The **density** of task $i$ is $d_i=1/a_i$, and
$\mathbf d=(d_1,\ldots,d_n)$ is the **density vector**, ordered
$d_1\geq\cdots\geq d_n>0$. The **density** and **potential** of the instance are
$\mathrm D(A)=\sum_{i=1}^n d_i$ and
$\mathrm P(A)=\sum_{i=1}^{n-1}d_i=\mathrm D(A)-d_n$, respectively.

A word $w$ using exactly the first $n$ letters represents a **periodic schedule**
$S_w$ by repeating the word indefinitely. For example, `AABACB` repeats tasks
1, 1, 2, 1, 3, 2. A schedule **dominates** an instance if it is valid for that
instance. The **dominance box** $B(w)$ is the product of the admissible task-density
intervals for $S_w$, before imposing the ordering of densities. A finite family
of words dominates a domain if every density vector in that domain belongs to
at least one of their dominance boxes.

## Installation

Run the commands below from the project directory. Python 3.10 or newer is
required. Install the Python dependencies, including pySMT and Z3, with
[uv](https://docs.astral.sh/uv/):

```sh
uv sync
```

The C++ programs require CMake 3.16 or newer, a C++20 compiler, and Boost with
program_options. These dependencies are installed separately; on Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libboost-dev libboost-program-options-dev
```

Build the programs in Release mode:

```sh
uv run cpp-build
```

Alternatively, invoke CMake directly:

```sh
cmake -S cpp_src -B cpp_build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp_build --parallel
```

## Verifying the supplied proofs

After building the C++ programs, run:

```sh
uv run check-proofs-5-6
uv run check-proofs-17-20
```

Each checker recomputes the dominance box of every stored word, then uses pySMT
with Z3 to check domination of the required domain. The SMT query asks for a
density vector in the domain outside all the dominance boxes. An unsatisfiable
answer proves domination; a satisfying assignment gives an undominated density
vector. The queries use linear real arithmetic with exact rational coefficients,
preserving strict and inclusive endpoints.

The checkers do not search for additional words and return a nonzero exit status if
any check fails. They report word counts, maximum word lengths, elapsed time,
and the largest ratio of word length to `2**dimension`.

Following the paper, $\mathcal D_n$ and $\mathcal P_n$ denote the density and
potential domains for the 5/6 bound. Superscript $(4)$ denotes the domains for
the 17/20 bound with minimum period at least 4. A certificate file `Dn.txt` or
`Pn.txt` specifies a finite family of words dominating the corresponding domain.
The exact domains checked are listed below; each row also imposes the stated
upper bound on **every** task density.

| Proof directory | Files | Domain | Quantity bounded | Bound | Maximum task density | Additional restrictions |
| --- | --- | --- | --- | --- | --- | --- |
| [proof_5_6](packing_plans/proof_5_6/) | D1.txt–D8.txt | $\mathcal D_n$, $1\leq n\leq8$ | Density | 5/6 | 1 | None |
| [proof_5_6](packing_plans/proof_5_6/) | P9.txt | $\mathcal P_9$ | Potential | 5/6 | 1 | None |
| [proof_17_20](packing_plans/proof_17_20/) | D1.txt–D9.txt | $\mathcal D_n^{(4)}$, $1\leq n\leq9$ | Density | 17/20 | 1/4 | None |
| [proof_17_20](packing_plans/proof_17_20/) | P9.txt | $\mathcal P_9^{(4)}$ | Potential | 17/20 | 1/4 | $d_1\geq3/20$ |
| [proof_17_20](packing_plans/proof_17_20/) | P10.txt | $\mathcal P_{10}^{(4)}$ | Potential | 17/20 | 1/4 | $d_1<3/20$ and $d_9<3/40$ |

The paper combines these domination results with **folding** and **unfolding**
to prove the general schedulability theorems. In particular, `P10.txt` certifies
only the residual ten-task potential domain $\mathcal P_{10}^{(4)}$. Folding
covers its complement in the full ten-task potential domain, including the
boundaries $d_1=3/20$ and $d_9=3/40$. These dimension-reduction arguments are
separate from the certificate checks.

## Using the prover

List the available options:

```sh
uv run python pinwheel_scheduling_packing_prover.py --help
```

For example, verify domination of $\mathcal P_{10}^{(4)}$:

```sh
uv run python pinwheel_scheduling_packing_prover.py \
  --dimension 10 --potential-bound 17/20 --max-density 1/4 \
  --bound 'd_1<3/20' --bound 'd_9<3/40' \
  --certificate packing_plans/proof_17_20/P10.txt --recompute-boxes --max-steps 0
```

Repeat `--bound` to impose several restrictions on the ordered densities.
Supported operators are `<`, `<=`, `>`, and `>=`; quote expressions to protect
them from the shell. All bounds are parsed as exact fractions. `--min-density`
is strict, while `--max-density`, `--density-bound`, and `--potential-bound`
are inclusive. Use either `--density-bound` or `--potential-bound` to choose the
quantity bounded; if neither is supplied, the density bound is 5/6.

Certificate files contain one word per line, optionally followed by the
coordinate bounds of its dominance box, as in the paper's example
`AABACB,<1/2,<=1/3,<=1/6`. Here the first density bound is strict, while the
other two include their endpoints. Use `--recompute-boxes` to recompute dominance
boxes from the words; otherwise complete annotations are trusted.
`--max-steps 0` checks domination without searching for additional words.
An undominated density vector gives a nonzero exit status.

With a positive step limit, the prover tries to construct a dominating word for
each undominated density vector. On success, `--write-certificate FILE` writes all words
used, including supplied words. `--partial-certificate FILE` appends newly found words
as the search proceeds. The command-line options `--word`, `--certificate`, and
`--library` supply words or files of words.

The native tools can also be used directly to find a dominating word for a given
density vector or compute the dominance box of a word:

```sh
./cpp_build/packing_schedule_find 1/2 1/2
./cpp_build/packing_schedule_quality AABACB
```
