# scripts/run_clang_tidy.py
# PlatformIO extra script: runs clang-tidy on all compiled .cpp/.h files after build
# Generates compile_commands.json from PlatformIO's idedata.json

import json
import os
import subprocess
import sys


def generate_compile_commands(project_root, src_dir):
    """Generate compile_commands.json from PlatformIO's idedata.json."""
    idedata_path = os.path.join(project_root, ".pio", "build", "pico-settings", "idedata.json")
    if not os.path.exists(idedata_path):
        return None

    with open(idedata_path) as f:
        data = json.load(f)

    # includes is a dict with keys like 'build', 'compatlib', 'toolchain'
    includes_data = data.get("includes", {})
    if isinstance(includes_data, dict):
        all_includes = []
        for key in includes_data:
            val = includes_data[key]
            if isinstance(val, list):
                all_includes.extend(val)
            elif isinstance(val, str):
                all_includes.append(val)
    else:
        all_includes = includes_data if isinstance(includes_data, list) else []

    defines = data.get("defines", [])
    cxx_flags = data.get("cxx_flags", [])
    if isinstance(cxx_flags, str):
        cxx_flags = cxx_flags.split()
    cxx_path = data.get("cxx_path", "clang++")

     # Build include flags
    include_args = []
    for path in all_includes:
        if os.path.isabs(path):
            include_args.extend(["-I", path])
        else:
            include_args.extend(["-I", os.path.join(project_root, path)])

     # Build define flags
    for d in defines:
        include_args.append("-D" + d)

     # Add standard compiler flags
    if isinstance(cxx_flags, list):
        include_args.extend(cxx_flags)
    src_files = []
    for root, _dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(('.cpp', '.h', '.hpp', '.c')):
                src_files.append(os.path.join(root, f))

    commands = []
    for src in src_files:
        commands.append({
            "directory": project_root,
            "command": "%s %s %s" % (cxx_path, " ".join(include_args), src),
            "file": src
        })

    cc_json_path = os.path.join(project_root, "compile_commands.json")
    with open(cc_json_path, 'w') as f:
        json.dump(commands, f, indent=2)

    return cc_json_path


def run_clang_tidy(env=None):
    # Determine project root (where this script's parent "scripts/" lives)
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cc_json = generate_compile_commands(project_root, os.path.join(project_root, "src"))

    # Get source files from PlatformIO env or default to src/
    if env is not None:
        src_dir = env.get("SRC_DIR")
    else:
        src_dir = os.path.join(project_root, "src")

    src_files = []
    for root, _dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(('.cpp', '.h', '.hpp', '.c')):
                src_files.append(os.path.join(root, f))

    if not src_files:
        print("[lint] No source files found to lint")
        return

    # Run clang-tidy with the compile database and config file
    cmd = [
        "clang-tidy",
        "--config-file=" + os.path.join(project_root, ".clang-tidy"),
        "--quiet",
    ]
    if cc_json:
        cmd.extend(["-p", project_root])

    cmd += src_files

    print("[lint] Running clang-tidy on %d files..." % len(src_files))
    result = subprocess.run(cmd, cwd=project_root)

    if result.returncode != 0:
        print("[lint] clang-tidy found issues (exit code %d)" % result.returncode)
        # Don't fail the build - lint is informational
    else:
        print("[lint] clang-tidy passed")


# PlatformIO extra script hook
if __name__ == "__main__":
    run_clang_tidy()
