# The Audio Engine is composed and outlives app screens

Music and Audiobooks share one Audio Engine and differ only by a Playback Policy, rather than inheriting from a generic player Activity. Audio has to keep playing while the listener reads, plays a game or lets the screen sleep, and this firmware deletes an Activity on every navigation, so anything inside an Activity's lifetime stops when its screen goes.

## Considered Options

- **A base player Activity that both apps inherit from.** Rejected: playback would end on navigation, or need a second mechanism to survive it.
- **One Audio Engine with two thin apps over a shared player screen.** Chosen. It also gives the one Queue the design requires, since there is only ever one engine to own it.
