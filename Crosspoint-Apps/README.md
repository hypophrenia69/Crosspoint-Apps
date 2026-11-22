# Apps for CrossPoint

*A calculator and a pomodoro timer on your e-reader.*

Small apps for the [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
firmware, built to sit next to the other add-ons (Dog-ear, To-do lists) in one firmware.

- **Calculator** – a pocket calculator: `+ - x /`, `%`, `+/-`, backspace, clear. Operations run as
  you type them (2 + 3 x 4 = 20), like a desk calculator.
- **Pomodoro** – focus and break timer: 25 min focus, 5 min short break, 15 min long break after
  4 rounds. All lengths can be changed on the screen and are remembered. Big seven-segment
  minutes, round markers, start / pause / skip / reset.

## Use it

**From the home screen:** the new **Apps** entry (between File Transfer and Settings, in the list
home and in the cover-grid home) opens a small launcher with the **To-do lists** (when the
Crosspoint-ToDoList add-on is in the same firmware), the **Calculator** and the **Pomodoro** timer.
Back from an app returns to the launcher, Back from the launcher to the home screen.

**From the file browser:** open the file browser and tap the file in the **Apps** folder (created on first start):

| File | Opens |
| --- | --- |
| `/Apps/Calculator.txt` | the calculator |
| `/Apps/Pomodoro.txt` | the pomodoro timer (the file also stores its settings) |

X4 Pro: tap the keys. X3 / X4: Left/Right/Up/Down move the frame, Confirm presses. Back leaves.

The timer shows whole minutes (the screen refreshes once a minute, to spare the e-ink and the
battery) and keeps the reader awake while it runs. When a phase ends the next one is ready:
press Start to go on. Leaving the screen stops the timer.

## Install

This is an add-on for the combined firmware in
[Crosspoint-Addons](https://github.com/Menrix/Crosspoint-Addons): add one line to its
`addons.txt` and run the build workflow:

```
<your-github-name>/Crosspoint-Apps      patch/apply_apps.py
```

The patch adds files and a few small hooks (start of an app, the home-screen entry, the two start
files, the update check). It changes nothing the other add-ons change; Dog-ear is not part of the
Apps screen on purpose.

## Tests

The logic (`patch/files/AppsCore.h`) has no firmware dependencies and is tested on a PC:

```
g++ -std=c++17 -Wall -Wextra -o /tmp/test_core test/test_core.cpp && /tmp/test_core
```

The screens (`AppsActivity.cpp`) are only compiled by the firmware build and have **not yet been
tried on a device**.

## License

MIT, see [LICENSE](LICENSE).
