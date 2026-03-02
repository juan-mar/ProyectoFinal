Import("env")

import os


project_src_dir = os.path.normcase(os.path.normpath(env.subst("$PROJECT_SRC_DIR")))
project_lib_dir = os.path.normcase(os.path.normpath(env.subst("$PROJECT_LIB_DIR")))
project_include_dir = os.path.normcase(os.path.normpath(env.subst("$PROJECT_INCLUDE_DIR")))


def _is_project_file(path: str) -> bool:
    normalized = os.path.normcase(os.path.normpath(path))
    return (
        normalized.startswith(project_src_dir)
        or normalized.startswith(project_lib_dir)
        or normalized.startswith(project_include_dir)
    )


def _warning_middleware(node):
    source_path = str(node)
    if _is_project_file(source_path):
        return node, {"CCFLAGS": ["-Wall", "-Wextra"]}

    return node, {"CCFLAGS": ["-w"]}


env.AddBuildMiddleware(_warning_middleware, "*")