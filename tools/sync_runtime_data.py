#!/usr/bin/env python3
import os
import shutil
import sys
from pathlib import Path

RUNTIME_MAP_FILES = [
    "world_map_tiles.csv",
    "world_roads.csv",
]

RUNTIME_TEMPLATE_ROOT = [
    "creatures.ini",
    "furniture.ini",
    "items.ini",
    "locations.ini",
    "locations.csv",
    "races.ini",
]

RUNTIME_AUDIO_ROOT = "audio"


def copy_file(src: Path, dst: Path) -> None:
    if not src.exists():
        raise FileNotFoundError(f"Required runtime file missing: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def copy_tree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def sync_runtime_data(out_dir: Path) -> None:
    repo_root = Path(__file__).resolve().parent.parent
    source_root = repo_root / "master_data"
    target_root = repo_root / out_dir / "data"

    if not source_root.exists():
        raise FileNotFoundError(f"Source data folder not found: {source_root}")

    if target_root.exists():
        shutil.rmtree(target_root)
    target_root.mkdir(parents=True, exist_ok=True)

    # Copy runtime audio assets.
    audio_src = source_root / RUNTIME_AUDIO_ROOT
    copy_tree(audio_src, target_root / RUNTIME_AUDIO_ROOT)

    # Copy runtime templates and template root files.
    templates_src = source_root / "templates"
    templates_dst = target_root / "templates"
    templates_dst.mkdir(parents=True, exist_ok=True)

    for name in RUNTIME_TEMPLATE_ROOT:
        copy_file(templates_src / name, templates_dst / name)

    # Copy full template subdirectories except `maps`, which is handled manually.
    for entry in templates_src.iterdir():
        if not entry.is_dir() or entry.name == "maps":
            continue
        copy_tree(entry, templates_dst / entry.name)

    # Copy only the runtime map files needed by the engine.
    maps_dst = templates_dst / "maps"
    maps_dst.mkdir(parents=True, exist_ok=True)
    for file_name in RUNTIME_MAP_FILES:
        copy_file(templates_src / "maps" / file_name, maps_dst / file_name)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: sync_runtime_data.py <out_dir>", file=sys.stderr)
        sys.exit(1)

    try:
        sync_runtime_data(Path(sys.argv[1]))
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
