# Music and Audiobooks: design

**Status:** Approved design, not started. This work targets a future device with
audio output. Don't build it for the Sticky.
**Last updated:** 2026-11-09
**Vocabulary:** [GLOSSARY.md](../../GLOSSARY.md)
**Related decision:** [ADR 0001](../adr/0001-audio-engine-outlives-app-screens.md)

## Context and scope

CrossPlay is e-reader firmware with games and small tools. The future device
adds audio output. This document specifies two apps for that device: Music, a
client for Navidrome, and Audiobooks, a client for Audiobookshelf.

Neither app streams. You browse and download on home Wi-Fi, then listen from the
SD card anywhere, including while you read or play a game.

## Goals

- Keep audio playing across app navigation and while the screen sleeps.
- Share one playback engine and one player screen between both apps.
- Play MP3, AAC, M4A, M4B, and FLAC files from the SD card.
- Sync Playlists, Book Positions, finished state, and play history with servers.

## Non-goals

- Streaming from a server.
- EQ, recording, Opus, Ogg, Bluetooth audio, or DRM-protected files.
- More than one Music Server or Audiobook Server.
- Automatic background sync.
- Device hardware design.

## Assumptions about the device

The design requires the `FREEINK_CAP_AUDIO` capability, the existing ESP32-S3
memory model or better, a touch screen, and an SD card. Physical media buttons
are useful but this design doesn't depend on them.

## Detailed design

### Components

```text
Audio Engine            one service, created at boot, independent of Activities
  Decoder               one per format, chosen by file type
  Output                wraps the SDK's AudioManager or a replacement
  Queue                 Tracks or Chapters
  Playback Policy       Music or Audiobooks
Music app               browse, Playlists, sync
Audiobooks app          browse, download, sync
Player screen           shared transport controls
Library, Sync           freestanding logic with host tests
```

Keep the Queue, Playback Policy, position rules, Track Match, and sync decisions
freestanding, with no Arduino types, so `host-tests/` can run them. This is the
pattern `src/apps_local/chess/ChessCore.h` follows.

### Audio Engine interface

Treat this as the starting contract. Adjust names to fit the code, but keep the
meaning.

```cpp
namespace audio {

enum class EngineState : uint8_t { Stopped, Loading, Playing, Paused, Error };

struct NowPlaying {
  EngineState state;
  uint16_t queueIndex;
  uint16_t queueCount;
  uint32_t positionMs;   // within the whole Audiobook, or within the Track
  uint32_t durationMs;
  uint16_t speedPercent;
};

class AudioEngine {
 public:
  bool load(const Queue& queue, const PlaybackPolicy& policy, uint32_t startMs);
  void play();
  void pause();
  void seekTo(uint32_t positionMs);
  void next();
  void previous();
  void setSpeedPercent(uint16_t percent);
  void setSleepTimerMinutes(uint16_t minutes);  // 0 cancels
  NowPlaying nowPlaying() const;                // safe from any task
};

}  // namespace audio
```

Run decoding on its own FreeRTOS task. Screens read `nowPlaying()` and send
commands. A screen never touches decoder state.

### Playback Policy

| Rule | Music | Audiobooks |
| --- | --- | --- |
| Start position | 0 | Book Position |
| At end of item | Next by shuffle and repeat | Next Chapter |
| At end of Queue | Stop, unless repeat is on | Mark finished |
| Saved state | Last Queue and position | Position and speed, per Audiobook |
| Speed | 100% only | Per Audiobook, 50% to 300% |
| Shuffle and repeat | Shuffle; off, all, or one | None |

### Queue behavior

- There is exactly one Queue.
- Starting playback in either app replaces it.
- Before an Audiobook's Queue is replaced, save its Book Position.
- A replaced Music Queue is discarded.

### Audiobooks

An Audiobook is one of these:

- One M4B or M4A file with embedded Chapters.
- A folder whose playable files are Chapters, in natural sort order.

If a folder contains a file the app can't play, flag the Audiobook on its detail
screen. Never skip a Chapter silently. Position is a single offset from the start
of the Audiobook, so it survives moving between Chapters.

Save a Book Position on pause, on Chapter change, on sleep-timer expiry, and every
30 seconds while playing. Don't write when the value hasn't changed.

When playback reaches the end, mark the Audiobook finished and clear its Book
Position. Opening it again starts from the beginning.

### Library

| Path | Contents |
| --- | --- |
| `/Music` | Tracks, in any folder structure |
| `/Audiobooks` | One file or folder per Audiobook |

Hide files neither app can play. Music offers folders and Playlists offline, and
server albums and artists online. A tag index is out of scope for v1.

### Screens

Follow `CLAUDE.md`: render three layout variants of each new screen before
building the winner. E-paper refresh is slow, so:

- Use tap-to-seek in fixed steps, not a dragged slider.
- Update the position display no more often than once a minute while playing.
- Show play state immediately, and let time values lag.

| Screen | Contents |
| --- | --- |
| Music home | Folders, Playlists, Server, Sync |
| Audiobooks home | In progress, All, Server, Sync |
| Player | Title, position, previous, play or pause, next, back and forward steps, sleep timer; speed for Audiobooks; shuffle and repeat for Music |
| Audiobook detail | Chapters, resume, remove download, unplayable-file flag |
| Download prompt | Items, total size, free space afterward |
| Conflict prompt | Both versions with last-edited times |

Provide a compact now-playing indicator in the Home and reader status areas.

### Data on the SD card

Write each file to a temporary name, then rename it, as game saves do.

| Path | Format | Contents |
| --- | --- | --- |
| `/.crosspoint/audio/playlists/<id>.m3u8` | M3U8, paths relative to `/Music` | One Playlist |
| `/.crosspoint/audio/playlists/index.json` | JSON | Name, server ID, last-edited time, last-synced version, state |
| `/.crosspoint/audio/matches.json` | JSON | Relative path to server ID |
| `/.crosspoint/audio/positions.json` | JSON | Per Audiobook: position ms, speed, finished, updated ms, server ID |
| `/.crosspoint/audio/queue.json` | JSON | Last Queue, position, shuffle, repeat |
| `/.crosspoint/audio/plays.jsonl` | One JSON object per line | Server ID, played-at ms |
| `/.crosspoint/audio/servers.json` | JSON | Server URLs and logins |

Add `servers.json` to `SENSITIVE_FILES` in `src/network/ProtectedPaths.h` so the
web file manager can't serve it.

### Network rules

All server work is a manual action on home Wi-Fi. Use `HttpDownloader`, write
downloads to a temporary file, and rename on success. Pause or refuse downloads
that would leave less than 64 MB free. Show progress as a count and a percentage.

### Music Server (Subsonic API)

Authenticate each request with `u`, `t`, `s`, `v=1.16.1`, `c=CrossPlay`, and
`f=json`, where `t` is the MD5 of the password followed by the random salt `s`.

| Purpose | Endpoint |
| --- | --- |
| Check login | `ping` |
| Browse | `getArtists`, `getArtist`, `getAlbumList2`, `getAlbum` |
| Playlists | `getPlaylists`, `getPlaylist` |
| Change Playlists | `createPlaylist`, `updatePlaylist` |
| Download an original | `download` |
| Report a play | `scrobble` with `id`, `time`, and `submission=true` |

#### Track Match

1. Compare the path relative to the library root.
2. Otherwise compare artist, title, and duration within 2 seconds.
3. Save the result so later syncs skip matching.

If several Tracks match in step 2, treat it as no match.

#### Playlist sync

For each Playlist on either side, apply the first row that matches:

| Condition | Action |
| --- | --- |
| Tracks missing on the device | Show the download prompt |
| Prompt accepted, and everything fits | Download, then continue the sync |
| Prompt accepted, and it doesn't fit | Refuse. Copy nothing |
| Prompt declined | Mark Not-on-device. Don't ask again until the server copy changes |
| Tracks missing on the server | Mark Device-only and list the blocking Tracks |
| Changed on both sides since last sync | Show the conflict prompt |
| Changed on one side | Copy that side to the other |
| Unchanged | Do nothing |

Detect change by comparing each side with the last-synced version in `index.json`.

### Audiobook Server (Audiobookshelf API)

The public reference at `api.audiobookshelf.org` says it's out of date. Confirm
every route against your running server before you build this.

| Purpose | Route | Confidence |
| --- | --- | --- |
| Log in | `POST /login` with `username`, `password`; token in `user.token` | Documented |
| Authenticate | `Authorization: Bearer <token>` | Documented |
| Libraries | `GET /api/libraries` | Documented |
| Items | `GET /api/libraries/<id>/items` with `limit`, zero-based `page`; compare with `total` | Documented |
| One item | `GET /api/items/<id>?expanded=1`; use `media.chapters`, `media.audioFiles`, `media.duration` | Partly documented |
| Download | `GET /api/items/<id>/download` or a per-file route | **Unverified** |
| Read position | `GET /api/me/progress/<id>` | **Unverified** |
| Write position | `PATCH /api/me/progress/<id>` | **Unverified** |

Progress records carry `currentTime` and `duration` in seconds, `progress` as a
fraction from 0 to 1, `isFinished`, and `lastUpdate` in milliseconds. Example data
shows these fields, but the units are inferred.

Sync rules:

- Book Position and finished state: the most recent `lastUpdate` wins, with no
  prompt. The device needs a correct clock, so skip the sync and tell the user if
  the time isn't set.
- Downloads are whole Audiobooks only.
- A copied-over Audiobook links to a server item when title, author, and total
  duration within 5 seconds all match.

### Removing downloads

Removing a download deletes only the device copy and keeps its Book Position. No
device action deletes anything on a server except a synced Playlist edit.

### Error handling

| Failure | Behavior |
| --- | --- |
| Corrupt or unsupported file during playback | Stop, show the file name, and leave the Queue in place |
| SD card removed | Pause and show an error |
| Audio underrun | Fill the buffer and continue; log it |
| Server unreachable or login rejected | Report it and keep local state |
| Interrupted download | Delete the temporary file and report it |

### Testing plan

Host tests, with no audio hardware:

- Queue, shuffle, repeat, and next and previous at both ends.
- Book Position save rules and finished state.
- Natural sort of Chapter files.
- Every row of the Playlist sync table.
- Track Match, including ambiguous matches.
- Most-recent-wins for positions, including equal timestamps.
- Space check boundaries.

Device tests are the acceptance list at the end of this document.

## Cross-cutting concerns

**Security.** Treat `servers.json` as a credential store. The Sticky's `HttpDownloader`
behaviour for plain HTTP applies, so use HTTPS for any server you reach beyond a
trusted network.

**Memory.** Measure decoder state and the read-ahead buffer. SD reads into PSRAM
can be slow on SDMMC (see the comment in `WikipediaPack.cpp`), so buffer in internal
RAM or bounce through it.

**Power.** Playing with the screen off rules out the deep sleep the firmware
normally enters. Add a light-sleep or awake audio state, and measure battery life.

**Concurrency.** `HalStorage` serializes SD access, so reader and app I/O compete
with the audio task.

## Alternatives considered

- **A base player Activity with subclasses.** Rejected, because Activities are
  deleted on navigation. See ADR 0001.
- **Streaming from the servers.** Rejected. It needs Wi-Fi during playback, and the
  device is meant to work anywhere.
- **Tag-indexed music library in v1.** Deferred, because folders work immediately
  and match how you load files.
- **Latest-wins for Playlists.** Rejected, because it can silently lose edits.

## Implementation plan

Each step is independently testable.

1. Freestanding Queue, Policy, position, and sync rules, with host tests.
2. Audio Engine plays one MP3 while you navigate between screens. Settle the risks
   listed under Open questions here.
3. M4B and folder Audiobooks, Chapters, speed.
4. Music app: folders, Playlists, shuffle, repeat, sleep timer.
5. Music Server: browse, download, Playlist sync, scrobbles.
6. Audiobook Server: browse, download, position sync.

When adding `AudioManager` to `lib_deps`, also add its task to
`scripts_local/stack_budget.py`.

## Open questions

Resolve these by measuring in step 2:

- Does audio stay unbroken during e-paper refreshes, game searches, and reader I/O?
- What read-ahead buffer size is enough?
- Does the SDK's `AudioManager` fit this design?
- Can the chip change speed without changing pitch on top of decoding FLAC or AAC?

## Acceptance criteria

- [ ] A Track keeps playing from Music into the reader and back without a gap.
- [ ] An Audiobook resumes at its Book Position after a restart.
- [ ] A position set on your phone appears on the device after sync, and the reverse.
- [ ] A server Playlist with missing Tracks prompts with correct sizes and free space.
- [ ] A Playlist that doesn't fit isn't partly copied.
- [ ] Plays made offline reach Navidrome at the next sync.
- [ ] Removing a download leaves the server unchanged.
- [ ] The sleep timer pauses playback and saves the Book Position.
