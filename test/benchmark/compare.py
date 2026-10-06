"""Compare identical public-API workloads in isolated baseline/current builds."""

import argparse
import json
from pathlib import Path
import platform
import re
import shutil
import statistics
import subprocess
import time


BASELINE = "824a2acd2886aa9e0ee3d8c390ac5e70b3fa18c6"
WORKLOADS = {
    "set_strings": 5000,
    "string_defaults": 5000,
    "custom_reads": 5000,
    "insert_sections": 3000,
    "merge_documents": 64,
    "serialize_documents": 128,
}


def capture(command, **kwargs):
    return subprocess.check_output(command, text=True, **kwargs).strip()


def compare(source, output, compiler):
    output.mkdir(parents=True, exist_ok=True)
    baseline_include = output / "baseline" / "include"
    header = baseline_include / "ini_manager" / "ini_manager.hpp"
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(capture([
        "git", "show", f"{BASELINE}:include/ini_manager/ini_manager.hpp",
    ], cwd=source) + "\n", encoding="utf-8")
    flags = ["-std=c++26", "-O3", "-DNDEBUG"]
    summary = {
        "baseline": BASELINE,
        "current": capture(["git", "rev-parse", "HEAD"], cwd=source),
        "compiler": capture([compiler, "--version"]),
        "valgrind": capture(["valgrind", "--version"]),
        "platform": platform.platform(),
        "flags": flags,
        "timing_scope": "workload including setup; excludes process startup",
        "allocation_scope": "whole process including setup and reporting",
        "builds": {},
    }
    executables = {}
    for label, include in [("baseline", baseline_include), ("current", source / "include")]:
        executable = output / f"workloads-{label}"
        command = [compiler, *flags, "-I", str(include),
                   str(source / "test/benchmark/workloads.cpp"), "-o", str(executable)]
        compile_samples = []
        for _ in range(3):
            start = time.perf_counter_ns()
            subprocess.run(command, check=True)
            compile_samples.append(time.perf_counter_ns() - start)
        executables[label] = executable
        summary["builds"][label] = {
            "compile_command": command,
            "compile_ns": compile_samples,
            "median_compile_ns": statistics.median(compile_samples),
            "executable_bytes": executable.stat().st_size,
            "workloads": {},
        }
    for name, iterations in WORKLOADS.items():
        timings = {label: [] for label in executables}
        checksums = set()
        # Alternate order to reduce systematic bias from runner load and warm caches.
        for sample in range(6):
            order = list(executables)
            if sample % 2:
                order.reverse()
            for label in order:
                result = json.loads(capture([str(executables[label]), name, str(iterations)]))
                checksums.add(result["checksum"])
                timings[label].append(result["elapsed_ns"])
        for label, executable in executables.items():
            log = output / f"{label}-{name}.valgrind.txt"
            result = json.loads(capture([
                "valgrind", "--tool=memcheck", "--leak-check=no", "--error-exitcode=99",
                f"--log-file={log}", str(executable), name, str(iterations),
            ]))
            checksums.add(result["checksum"])
            match = re.search(
                r"total heap usage: ([\d,]+) allocs, ([\d,]+) frees, ([\d,]+) bytes allocated",
                log.read_text(encoding="utf-8"),
            )
            if not match:
                raise RuntimeError(f"Missing allocation summary in {log}")
            allocations, frees, allocated_bytes = (int(value.replace(",", "")) for value in match.groups())
            summary["builds"][label]["workloads"][name] = {
                "iterations": iterations,
                "elapsed_ns": timings[label],
                "median_elapsed_ns": statistics.median(timings[label]),
                "checksum": result["checksum"],
                "allocations": allocations,
                "frees": frees,
                "allocated_bytes": allocated_bytes,
            }
        if len(checksums) != 1:
            raise RuntimeError(f"Behavior mismatch in {name}: {checksums}")
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is None or shutil.which("valgrind") is None:
        parser.error("A supported C++26 compiler and Valgrind are required")
    compare(Path(__file__).resolve().parents[2], args.output.resolve(), compiler)


if __name__ == "__main__":
    main()
