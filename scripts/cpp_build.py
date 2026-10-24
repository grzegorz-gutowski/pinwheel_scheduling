#!/usr/bin/env python3

"""Build the project's C++ executables with CMake."""

from pathlib import Path
import subprocess
import sys

def main() -> int:
    build = "cpp_build"
    source = "cpp_src"
    try:
        subprocess.run(
            ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", str(build), "--config", "Release", "--parallel"],
            check=True,
        )
    except FileNotFoundError:
        print("CMake is required to build the C++ programs; install cmake first.", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        return error.returncode
    return 0

if __name__ == "__main__":
    sys.exit(main())
