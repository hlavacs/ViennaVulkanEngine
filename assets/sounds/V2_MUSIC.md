# Soundtrack restored from VVE V2

These files were copied without modification from the ViennaVulkanEngine `V2`
branch, commit `73890d13adcf8422879b58e6199f0675dd6d0514`:

| File | Original attribution |
|---|---|
| `dance.mp3` | The background track used by V2's `examples/game/game.cpp`; V2's license file does not name its author. |
| `ophelia.mp3` | “ophelias symphony”, Tomas PhUsIoN (2006); V2 identifies Creative Commons Attribution 2.5. Original source: http://ccmixter.org/media/files/phusion/6442 |
| `getout.ogg` | “never get out”, tone_group (2007); V2 identifies Creative Commons Attribution 2.5. Original source: http://ccmixter.org/media/files/seb_grenning/8920 |

The original `license.txt` is preserved alongside the music, including its full
license text and credits for V2's other audio assets. Its entries for tracker
music and other files do not describe the soundtrack's three files uniformly.
The original source did not provide a separate attribution for `dance.mp3`.

V3's Crate Collector and Relay Siege start with Dance and offer Ophelia and Never
Get Out in the music selector. Each track repeats until switched or the game exits.
SDL3_mixer's `mpg123` feature decodes MP3; its built-in Vorbis decoder handles OGG.
Both games use only VVE's public `AudioSystem` facade to load and control playback.
