# Pick-up checklist, 2026-11-08

Branch: `feature/shogi-xiangqi-fork-updates`, pushed over SSH. No pull request opened, and
`xteink` is unchanged. Gate: `check.sh --committed` was green, run with
`CHECK_ALLOW_UNDO=1` because deleting the uploaded `.patch` looks like a revert.

- [ ] Optional: `git remote set-url origin git@github.com:rahulsanjay18/crossplay.git`, since `origin` is HTTPS and has no login here.

## Try on the device

- [ ] Flash with `scripts_local/flash-sticky.ps1 -BuildOnly` first, then without it.
      The PowerShell script has never been run. The Linux one is tested against
      fakes only. If it says no serial data was received, hold BOOT and tap RESET.
- [ ] Play a game of Shogi and Xiangqi. Menus were seen rendering in the simulator;
      no game has been played on hardware.
- [ ] Open a heavy app (Go, Hex, Underhand, Shogi) and read heap on a `-e sticky`
      build through the serial bridge's HEAP command. The audit's runtime figures
      are from the simulator and static sizes.
- [ ] Confirm the device reports the 512 GB card's real capacity. It must be exFAT.

## Decide

- [ ] Merge the branch? Open a pull request, or merge to `xteink` directly.
- [ ] Per-app build flags. The audit shows removing a shelf row is enough. Proposed:
      one list of disabled titles per env, read at the top of `Shelf.cpp`. Needs a
      rule for the doc-claims tests, and a decision on Notes and Wallpapers, which
      the web server and sleep screen still reach.
- [ ] Reclaim permanent RAM by allocating on open: Go 36 KB, Toy Battle 24 KB,
      xkcd 7 KB, Shogi 7 KB. This also covers the old MoveList TODO.
- [ ] What to move to SD, if anything. Largest: reader fonts about 2.2 MB, UI strings
      404 KB, hyphenation 352 KB, Underhand 277 KB. See README.md.

## Needs you

- [ ] Publish a release on the fork. Updates now look only at
      `rahulsanjay18/crossplay` and find nothing until one exists, with an asset
      named `firmware-sticky.bin` and a tag newer than the device's version.
- [ ] Other upstream services: Wikipedia, xkcd, trivia, wallpapers, Instapaper and
      Study still use `ma-r-s.com` hosts, which receive device headers. Only the
      update check was changed.
- [ ] BookLore OPDS bug: run the `curl` from the handoff notes against the server.

## Set up on this Linux machine during the audit

- PlatformIO in `~/.local/share/pio-venv`, `clang-format` 21 through `uv tool`.
- Git remote `crosspoint` (upstream CrossPoint), which two suites compare against.
- `.pio-cache/` is untracked in the repo root and was left out of the commits.
