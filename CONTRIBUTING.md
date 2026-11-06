# Contributing

Bug reports are most useful with `openciv4.log` and an autosave from before the problem: the
engine is deterministic, so a save replays the same turn. The issue forms ask for both.

## Clean room

openciv4 is written from public documentation, the officially released SDK headers and what
can be observed of the original game running. Do not contribute code or knowledge taken from
disassembling or decompiling the original executables, and do not commit any of the game's
files (art, XML, Python, saves).

## Pull requests

* Build and test as in the [README](README.md#building-windows-x64); `ctest` passes.
* Game results stay the same: with a fixed seed an AI autoplay prints the same sync checksum
  every turn before and after the change, unless the change fixes what made them differ.
  For example, `oc4core.exe --turns 100 --map Fractal --size STANDARD --seed 7`.
* Original saves keep loading.
* Changes to the game core go to [openciv4-dll](https://github.com/bssth/openciv4-dll) (the
  `gamecore` submodule) and follow its rules.
* Comments explain what the code cannot say itself; no history of the change in them, that
  is what the commit message is for.
