Import("env")

import os
import sys


def resolve_mklittlefs():
    exe_name = "mklittlefs.exe" if sys.platform.startswith("win") else "mklittlefs"

    tool_dir = None
    try:
        tool_dir = env.PioPlatform().get_package_dir("tool-mklittlefs")
    except Exception:
        tool_dir = None

    if tool_dir:
        candidate = os.path.join(tool_dir, exe_name)
        if os.path.exists(candidate):
            return candidate

    local_candidate = os.path.join(env.subst("$PROJECT_DIR"), exe_name)
    if os.path.exists(local_candidate):
        return local_candidate

    return exe_name


tool_path = resolve_mklittlefs()
print("Replace MKSPIFFSTOOL with %s" % tool_path)
env.Replace(MKSPIFFSTOOL=tool_path)
