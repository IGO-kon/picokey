# Contributing to picokey

Thanks for your interest in improving picokey.

## Before you start

- Search existing issues and pull requests first.
- For bugs, include logs from `screen /dev/ttyACM0 115200` when possible.
- For behavior changes, describe expected and current behavior clearly.

## Local setup

1. Install prerequisites listed in `README.md`.
2. Build the firmware:

```bash
cmake -S . -B build -DPICO_BOARD=pico2_w
cmake --build build -j
```

3. Flash `build/picokey.uf2` to the Pico board and test on hardware.

## Coding guidelines

- Keep changes focused and small.
- Prefer readable constants for gesture tuning.
- Keep logging concise and useful.
- Do not break existing keyboard behavior while changing pointer gestures.

## Pull request checklist

- [ ] Build succeeds locally.
- [ ] Behavior tested on hardware (keyboard and touchpad if relevant).
- [ ] README updated when user-visible behavior changes.
- [ ] Commit message explains what changed and why.

## Commit style

Use short, descriptive commit messages. Example:

- `Tune two-finger tap threshold for right-click`

## Questions

Open a GitHub issue with the `question` label if you need clarification.
