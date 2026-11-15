# Pick-up checklist

Last updated: 2026-11-09. Your local tree is on `feature/guest-hotspot`.

## Decide

- [ ] Commit the guest hotspot and open a pull request. It's uncommitted, because
      you haven't approved a commit for it. `check.sh` passed everything, including
      the three release builds, except the intermittent `ship` suite described under
      Known issues.
- [ ] Commit the specs, ADR, and `GLOSSARY.md`. They're untracked in the same tree.
- [ ] Review [pull request 3](https://github.com/rahulsanjay18/crossplay/pull/3),
      the ten Chess levels.
- [ ] Commit Markdown notes and open a pull request. The work is uncommitted on
      `feature/notes-markdown` in `../crossplay-wt/notes-markdown`, and `check.sh`
      is green with all three release builds.
- [ ] Try Markdown notes on the Sticky: tap a line to edit it, and use the MD key
      on the keyboard.
- [ ] Tell me about the other apps you want specced.
- [ ] Later: a better notes system and fuller Markdown.

## Test on the Sticky

Flash `feature/guest-hotspot`, then open File Transfer and choose Share Files.

- [ ] Scan the QR code with a phone and confirm that it joins.
- [ ] Confirm that restarting Share Files changes the password.
- [ ] Put a file in `/share` and download it from the guest page.
- [ ] From a laptop on the hotspot, run the following. `/share/download` and
      `/api/share` should work for files in `/share` only. Everything else should
      return 403, 404, or a 302 redirect to the guest page. No response should
      contain file data or settings.

```bash
B=http://192.168.4.1
for p in '/api/share?path=/share' '/api/share?path=/' \
    '/api/share?path=/share/../books' '/share/download?path=/books/x.epub' \
    '/share/download?path=/share/%2e%2e/books/x.epub' \
    '/share/download?path=/share/..%20/books/x.epub' \
    '/download?path=/books/x.epub' /files /api/files /api/wifi /api/settings /api/opds; do
  printf '%s -> ' "$p"; curl -s -o /dev/null -w '%{http_code}\n' "$B$p"
done
for p in /delete /upload /rename /move /mkdir /api/relay /api/fetch; do
  printf 'POST %s -> ' "$p"; curl -s -o /dev/null -w '%{http_code}\n' -X POST "$B$p"
done
```

Replace `192.168.4.1` if the Share Files screen shows another address.

- [ ] Play a game of Chess at a few new levels, and confirm that EXPERT's thinking
      time is acceptable.
- [ ] Try `scripts_local/flash-sticky.ps1 -BuildOnly`, which has never been run.

## Known issues

- The `ship` suite fails about half the time, on unmodified `xteink` too, so
  `check.sh` can report `failed` at random. The script's `pipefail` with `grep -q`
  is the likely cause. It's a small fix if you want it.
- Create Hotspot now requires a password, but anyone who has it still gets the full
  file manager. Share it only with people you trust.

## Future device

- [Music and Audiobooks](audio-apps.md). Confirm the Audiobookshelf download and
  progress routes against your server before anyone builds it.
