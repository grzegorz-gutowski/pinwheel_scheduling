# Pinwheel Scheduling

Install the Python dependencies with [uv](https://docs.astral.sh/uv/):

```sh
uv sync
```

Run the scripts from the project directory:

```sh
uv run python pinwheel_scheduling_packing_prover.py --help
```

The project requires Python 3.10 or newer and includes pySMT with the Z3 solver.
The project installs a `build-cpp` command using uv's
[project entry points](https://docs.astral.sh/uv/concepts/projects/config/#command-line-interfaces).

Build the C++ programs with CMake 3.16 or newer, a C++20 compiler, and the
Boost development libraries (including Program_options). On Debian/Ubuntu,
install these with `sudo apt install build-essential cmake libboost-dev libboost-program-options-dev`.

From the project directory, configure and build both programs in Release mode:

```sh
uv run cpp-build
```

Alternatively, invoke CMake directly:

```sh
cmake -S cpp_src -B cpp_build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp_build --parallel
```

Run the resulting executables from the project directory:

```sh
./cpp_build/packing_schedule_find 1/2 1/2
./cpp_build/packing_schedule_quality AB
```

The C++ toolchain and Boost must be installed separately from the Python dependencies.

Check the supplied proofs for dimensions 1–9 and the potential variant in dimension 9:

```sh
uv run check-proofs
```

Build the C++ programs first. The checker recomputes schedule bounds and checks
coverage at density (or potential) at most `5/6`, without generating new plans.
It exits with a nonzero status if any proof fails.
