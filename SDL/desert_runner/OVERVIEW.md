# Desert Runner

An endless runner written in **C++17** with **SDL3**. A little blue character
runs across a scrolling desert; jump over cacti and duck under birds for as long
as you can while the game accelerates.

Everything is drawn procedurally with filled rectangles — **no font or image
assets are required.** The only dependency is SDL3 itself.

![gameplay is rendered entirely from rectangles: player, cacti, birds, clouds, a scrolling ground, and a seven-segment score]

## How it works

- **Fixed player, moving world.** The player stays at a fixed x-position; the
  ground, clouds, and obstacles scroll left at the current game speed, which is
  the standard endless-runner illusion of forward motion.
- **Delta-time physics.** Movement is integrated against a per-frame `dt`
  (clamped to guard against hitches), so behavior is frame-rate independent.
  Gravity pulls the player down; ducking mid-air applies a stronger "fast fall".
- **Procedural obstacles.** A timer spawns cacti (jump over) and, once you've
  survived a few seconds, birds at head height (duck under, or jump). Spawn
  intervals are time-based so reaction time stays fair as speed rises.
- **Difficulty ramp.** Over the first ~55 seconds the speed rises from 380 to
  850 px/s, spawn gaps tighten, and birds become more frequent.
- **Collision.** Axis-aligned bounding-box test between a slightly forgiving
  player hitbox and each obstacle.
- **Seven-segment HUD.** The score and high score are rendered digit-by-digit
  from rectangles, which is why the game needs no font files.

## Ideas to extend it

- Persist the high score to a file between runs.
- Add sound with `SDL_AudioStream` (jump / hit cues).
- Swap the rectangle art for sprite sheets loaded via SDL_image.
- Add a second obstacle lane, coins, or power-ups.
