# Password hotspot and guest file sharing: design

**Status:** Implemented on `feature/guest-hotspot`. Needs a test on a real device.
**Last updated:** 2026-11-09

## Context and scope

The device can host its own Wi-Fi network. Before this change, that network was
open and served the full file manager, so anyone in radio range could read, change,
or delete files, and could reach settings and saved network details.

The intended use is that your phone downloads files over Tailscale, joins the
hotspot, and uploads them. Someone else then joins and downloads what you shared.

## Goals

- Require a password on every hotspot.
- Let guests list and download only what you put in `/share`.

## Non-goals

- Resuming interrupted downloads. `handleDownload` has no HTTP Range support.
- Remembered passwords or a configurable network name.
- Hosting a hotspot while connected to home Wi-Fi.

## Design

| | Create Hotspot | Share Files |
| --- | --- | --- |
| Purpose | Manage files with your own device | Let other people download |
| Server surface | `Full` | `GuestShare` |
| Readable | The card | `/share` only |
| Writable | Yes | Nothing |

### Password

Each hotspot start generates a new 10-character password with `esp_random`, using
a 32-symbol alphabet without look-alike characters. A guest who was given a
password can't return later or join an owner session.

The password appears on the screen as text and in the Wi-Fi QR code. It isn't logged
or stored.

### Guest routes

| Route | Response |
| --- | --- |
| `GET /` | Guest page |
| `GET /api/share?path=<dir>` | JSON listing |
| `GET /share/download?path=<file>` | The file |

Other paths return 404, or redirect to the guest page in hotspot mode, as captive
portals expect. The surface has no CORS, WebSocket, WebDAV, or developer routes.

### Path rules

`guestshare::isSharedPath` must accept both the raw argument and the normalized
path. It requires `/share` or a path under `/share/`, and rejects any segment that
is empty or starts with a dot after SdFat's name handling. SdFat skips leading
spaces and drops trailing dots and spaces, so `.. ` counts as a parent reference.
Listings always hide dot files, even when the owner's settings show them.

## Code

| File | Change |
| --- | --- |
| `src/network/GuestShareCore.h` | Password, QR payload, and path rules |
| `src/network/CrossPointWebServer.*` | `GuestShare` surface and handlers |
| `src/network/html/GuestSharePage.html` | Guest page |
| `src/activities/network/CrossPointWebServerActivity.*` | Password and surface choice |
| `src/activities/network/NetworkModeSelectionActivity.*` | Share Files row |
| `host-tests/guestshare/` | Rules and route-count tests |

## Alternatives considered

- **Restricting the `Full` surface for guests.** Rejected. Every future route would
  need remembering to block. A separate surface exposes nothing by default, as
  `WallpapersOnly` and `NotesOnly` do.
- **One remembered password.** Rejected, because anyone given it could rejoin later.

## Testing

Host tests cover the path rules, password generation, QR escaping, and the number
of guest routes. The simulator replaces the web server with its own, so it can't
test these routes. The device test is in
[the checklist](pickup-checklist.md).
