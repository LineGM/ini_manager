"""Compare consumer installation defaults without compiling or installing files."""

from pathlib import Path
import subprocess
import sys
import tempfile


def configure(cmake, root, library, with_library, prefix):
    root.mkdir()
    project = [
        "cmake_minimum_required(VERSION 4.0...4.4)",
        "project(install_defaults VERSION 1.0.0 LANGUAGES NONE)",
        # Only header installation rules are exercised; no compiler is needed.
        "set(CMAKE_SIZEOF_VOID_P 8)",
    ]
    if with_library:
        project += [
            "set(PROJECT_IS_TOP_LEVEL FALSE)",
            f'set(PROJECT_SOURCE_DIR "{library.as_posix()}")',
            "add_library(ini_manager_ini_manager INTERFACE)",
            "target_sources(ini_manager_ini_manager INTERFACE FILE_SET HEADERS",
            '  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"',
            '  FILES "${PROJECT_SOURCE_DIR}/include/ini_manager/ini_manager.hpp")',
            'include("${PROJECT_SOURCE_DIR}/cmake/install-rules.cmake")',
        ]
    else:
        project += ["include(GNUInstallDirs)"]
    project += [
        'file(WRITE "${CMAKE_BINARY_DIR}/libdir.txt" "${CMAKE_INSTALL_LIBDIR}")',
    ]
    (root / "CMakeLists.txt").write_text("\n".join(project) + "\n", encoding="utf-8")
    binary = root / "out"
    subprocess.run([
        cmake, "-S", str(root), "-B", str(binary), f"-DCMAKE_INSTALL_PREFIX={prefix}",
    ], check=True, stdout=subprocess.DEVNULL)
    return (binary / "libdir.txt").read_text(encoding="utf-8")


def main():
    cmake, source = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="ini-install-dirs-") as temporary:
        root = Path(temporary)
        for index, prefix in enumerate(("/", "/usr", "/usr/local")):
            expected = configure(cmake, root / f"control-{index}", Path(source), False, prefix)
            actual = configure(cmake, root / f"consumer-{index}", Path(source), True, prefix)
            assert actual == expected, (prefix, expected, actual)


if __name__ == "__main__":
    main()
