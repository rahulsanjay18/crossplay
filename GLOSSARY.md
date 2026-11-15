# CrossPlay

Personal e-reader firmware with games and small tools. This glossary starts with the audio apps.

## Language

### Audio

**Audio Engine**:
The single playback service on the device, which outlives any app screen.
_Avoid_: Generic player, base player

**Queue**:
The one ordered list of Tracks or Chapters the Audio Engine is playing.
_Avoid_: Now playing list

**Playback Policy**:
What distinguishes Music from Audiobooks to the Audio Engine: what plays next, what is remembered, and the default speed.
_Avoid_: Player subclass

**Track**:
One song file in the music library.
_Avoid_: Song file

**Audiobook**:
One book, stored as a single chaptered file or as a folder of audio files.
_Avoid_: Book (which means a text book in this firmware)

**Chapter**:
A section of an Audiobook: an embedded chapter, or one file in the Audiobook's folder.
_Avoid_: Part

**Book Position**:
Where listening stopped in an Audiobook, kept per Audiobook together with its speed.
_Avoid_: Bookmark, progress

**Playlist**:
A named, ordered list of Tracks, made on the device or synced from a Music Server.

**Music Server**:
A Navidrome or other Subsonic API server that Playlists sync with.
_Avoid_: Subsonic, backend

**Audiobook Server**:
An Audiobookshelf server that Audiobooks and Book Positions sync with.
_Avoid_: ABS, backend

**Track Match**:
The remembered link saying a Track on the device is a particular song on the Music Server.

**Complete Playlist**:
A Playlist whose every Track exists on both the device and the Music Server, the only kind that syncs.

**Device-only Playlist**:
A Playlist that cannot sync because the Music Server lacks at least one of its Tracks.

**Not-on-device Playlist**:
A Music Server Playlist the listener declined to copy because some of its Tracks are missing from the device.

**Finished Audiobook**:
An Audiobook played to its end, which has no Book Position and starts from the beginning when next opened.

### Songs

**Chart**:
One song's chords and lyrics, as a text or ChordPro file in the songs folder.
_Avoid_: Tab, song file

**Chord Line**:
A line of a chart holding only chords, read as sitting over the lyric line below it.

**Shape**:
The chord as fingered, which with a capo differs from the chord that sounds.
_Avoid_: Fingering

**Chart Capo**:
The capo a chart was written for, where it opens.

**Capo**:
Where the player's capo is now, which changes the Shapes shown but not the sound.

**Transpose**:
Moving the whole song up or down by semitones, which changes the sound.
_Avoid_: Key change

**Setlist**:
A named, ordered list of Charts played through in turn.
_Avoid_: Playlist (which belongs to Music)

**Score**:
One piece of sheet music, kept as a folder of page images.
_Avoid_: Sheet, PDF
