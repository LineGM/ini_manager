"""Exercise source selection without running a compiler or clang-tidy."""

import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    cmake, script = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="ini-tidy-") as temporary:
        root = Path(temporary)
        source = root / "build" / "checkout+(test)"
        binary = source / "out"
        binary.mkdir(parents=True)
        unit = source / "test" / "sample.cpp"
        unit.parent.mkdir()
        marker = binary / "invocations.jsonl"
        # Python acts as the analyzer and records its arguments. The fake TU is
        # Python source so this portable test needs no executable shell wrapper.
        unit.write_text(
            "import json, pathlib, sys\n"
            f"with pathlib.Path({str(marker)!r}).open('a') as output:\n"
            "    output.write(json.dumps(sys.argv[1:]) + '\\n')\n",
            encoding="utf-8",
        )
        generated = binary / "generated.cpp"
        external = root / "dependency.cpp"
        database = binary / "compile_commands.json"
        entries = [{"file": str(path)} for path in (unit, unit, generated, external)]
        command = [
            cmake,
            f"-DSOURCE_DIR={source}",
            f"-DBINARY_DIR={binary}",
            f"-DTIDY_COMMAND={sys.executable}",
            "-P",
            script,
        ]
        database.write_text(json.dumps(entries), encoding="utf-8")
        subprocess.run(command, check=True)
        calls = [json.loads(line) for line in marker.read_text().splitlines()]
        assert len(calls) == 1, calls
        header_filter = next(arg.split("=", 1)[1] for arg in calls[0]
                             if arg.startswith("--header-filter="))
        assert re.search(header_filter, str(source / "include" / "header.hpp"))
        for empty in ([], [{"file": str(generated)}]):
            database.write_text(json.dumps(empty), encoding="utf-8")
            result = subprocess.run(command, capture_output=True, text=True, check=False)
            assert result.returncode != 0
            assert "No project translation units" in result.stderr


if __name__ == "__main__":
    main()
