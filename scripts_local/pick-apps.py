#!/usr/bin/env python3
"""Choose which games and apps go into a firmware build, then build it.

    ./scripts_local/pick-apps.py                     # pick from a checklist, then build
    ./scripts_local/pick-apps.py --keep "CHESS,NOTES,SONGS"
    ./scripts_local/pick-apps.py --list              # what is on the shelf
    ./scripts_local/pick-apps.py --no-build          # only save the choice
    ./scripts_local/pick-apps.py --env sticky        # default gh_release_sticky

Each row left out is built with -DCROSSPLAY_OMIT_<NAME>, which takes it off the
shelf and out of the image. The choice is saved in crossplay-apps.local.txt
(one kept title per line) and is the starting point next time.

On Linux with whiptail the choice is a checklist. Anywhere else it is a numbered
list: type numbers to toggle them, then press Enter on an empty line.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHELF = os.path.join(REPO, "src", "apps_local", "Shelf.cpp")
CHOICE = os.path.join(REPO, "crossplay-apps.local.txt")
ROW = re.compile(r'\{"([^"]+)", &icon_[a-z_0-9]+, &([A-Za-z]+)Activity::create\}')


def shelf_items():
    """(folder, title, macro) for every shelf row, in shelf order."""
    text = open(SHELF, encoding="utf-8").read()
    apps_at = text.index("kApps[] = {")
    items = []
    for match in ROW.finditer(text):
        folder = "APPS" if match.start() > apps_at else "GAMES"
        items.append((folder, match.group(1), "CROSSPLAY_OMIT_" + match.group(2).upper()))
    return items


def saved_choice(items):
    titles = {title for _, title, _ in items}
    if not os.path.exists(CHOICE):
        return set(titles)
    kept = {line.strip() for line in open(CHOICE, encoding="utf-8") if line.strip()}
    return kept & titles


def pick_with_whiptail(items, kept):
    arguments = ["whiptail", "--title", "CrossPlay build", "--separate-output", "--checklist",
                 "Space toggles, Enter builds. At least one game and one app.", "30", "70", "22"]
    for folder, title, _ in items:
        arguments += [title, folder.lower(), "ON" if title in kept else "OFF"]
    result = subprocess.run(arguments, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        sys.exit("cancelled")
    return {line.strip() for line in result.stderr.splitlines() if line.strip()}


def pick_with_prompt(items, kept):
    kept = set(kept)
    while True:
        for number, (folder, title, _) in enumerate(items, 1):
            print(f"{number:3}  [{'x' if title in kept else ' '}]  {folder:5}  {title}")
        answer = input("Numbers to toggle (or 'all', 'none'), Enter to build: ").strip().lower()
        if not answer:
            return kept
        if answer == "all":
            kept = {title for _, title, _ in items}
            continue
        if answer == "none":
            kept = set()
            continue
        for part in re.split(r"[ ,]+", answer):
            if part.isdigit() and 1 <= int(part) <= len(items):
                title = items[int(part) - 1][1]
                kept ^= {title}


def find_pio():
    for candidate in (shutil.which("pio"), os.path.expanduser("~/.platformio/penv/bin/pio"),
                      os.path.expanduser("~/.local/share/pio-venv/bin/pio"),
                      os.path.expanduser("~/.platformio/penv/Scripts/pio.exe")):
        if candidate and os.path.exists(candidate):
            return candidate
    sys.exit("PlatformIO not found; pip install platformio")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--keep", help="comma-separated titles to keep, instead of the checklist")
    parser.add_argument("--list", action="store_true", help="print the shelf and the saved choice")
    parser.add_argument("--no-build", action="store_true", help="save the choice without building")
    parser.add_argument("--env", default="gh_release_sticky", help="PlatformIO environment")
    args = parser.parse_args()

    items = shelf_items()
    kept = saved_choice(items)
    if args.list:
        for folder, title, _ in items:
            print(f"[{'x' if title in kept else ' '}]  {folder:5}  {title}")
        return

    if args.keep is not None:
        wanted = {part.strip().upper() for part in args.keep.split(",") if part.strip()}
        known = {title.upper(): title for _, title, _ in items}
        unknown = sorted(wanted - set(known))
        if unknown:
            sys.exit("not on the shelf: " + ", ".join(unknown) + "  (see --list)")
        kept = {known[name] for name in wanted}
    elif shutil.which("whiptail") and sys.stdin.isatty():
        kept = pick_with_whiptail(items, kept)
    else:
        kept = pick_with_prompt(items, kept)

    for folder in ("GAMES", "APPS"):
        if not any(f == folder and title in kept for f, title, _ in items):
            sys.exit(f"keep at least one of the {folder.lower()}: a folder cannot be empty")

    with open(CHOICE, "w", encoding="utf-8") as out:
        out.write("".join(title + "\n" for _, title, _ in items if title in kept))
    omitted = [macro for _, title, macro in items if title not in kept]
    print(f"keeping {len(items) - len(omitted)} of {len(items)}; saved to {os.path.basename(CHOICE)}")
    if args.no_build:
        return

    environment = dict(os.environ)
    flags = " ".join("-D" + macro for macro in omitted)
    environment["PLATFORMIO_BUILD_FLAGS"] = (environment.get("PLATFORMIO_BUILD_FLAGS", "") + " " + flags).strip()
    result = subprocess.run([find_pio(), "run", "-d", REPO, "-e", args.env], env=environment)
    if result.returncode != 0:
        sys.exit("build failed")
    build = os.path.join(REPO, ".pio", "build", args.env)
    size = os.path.getsize(os.path.join(build, "firmware.bin"))
    print(f"\nbuilt {args.env}: firmware.bin is {size:,} bytes, in {build}")
    print("flash it with: ./scripts_local/flash-sticky.sh --skip-build --env " + args.env)


if __name__ == "__main__":
    main()
