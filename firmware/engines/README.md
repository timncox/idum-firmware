# Alternate Engines

Each production Alt-DUM build combines the stock-derived/shared sketch in
`firmware/idum/` with exactly one directory from here. Source files remain
separate in Git; `scripts/stage-sketch.sh` composes Arduino's required flat
sketch under `build/staged/<engine>/idum/`.

An engine directory owns its runner state and must provide the run/reset entry
points selected by `firmware/idum/c00_AlternateEngine.ino`. To add an engine:

1. Add its numeric ID to `firmware/idum/engine_config.h`.
2. Add `firmware/engines/<name>/` containing its `.ino` source.
3. Map the name to the ID in the Makefile and shell build wrappers.
4. Add its dispatch branch and simulator include.
5. Add engine-specific regression coverage.

Do not copy engine source into `firmware/idum/`; the staging step performs the
temporary composition for builds.
