#pragma once
// Calculator and pomodoro timer screens (added by the Crosspoint-Apps patch).
// They open from "/Apps/Calculator.txt" and "/Apps/Pomodoro.txt" in the file browser.
//
//  X4 Pro (touch): tap the keys / buttons.
//  X3 / X4 (keys): Left/Right/Up/Down move the frame, Confirm presses.
//  Back leaves.

#include <string>
#include <vector>

#include "AppsCore.h"
#include "activities/Activity.h"

class CalculatorActivity final : public Activity {
  std::string filePath;
  capps::Calculator calc;
  int selected = 15;  // key frame for button-only devices (starts on "5")
  int updatesSinceFull = 0;

 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
      : Activity("Calculator", renderer, mappedInput), filePath(std::move(path)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

class PomodoroActivity final : public Activity {
  enum class Id : uint8_t {
    StartPause, Skip, Reset,
    WorkMinus, WorkPlus, ShortMinus, ShortPlus, LongMinus, LongPlus, RoundsMinus, RoundsPlus
  };
  struct Button {
    int x, y, w, h;
    Id id;
    const char* label;
  };

  std::string filePath;
  capps::Pomodoro pomo;
  int selected = 0;           // button frame for button-only devices
  int lastMinutesShown = -1;  // the screen shows whole minutes; redraw when they change
  int updatesSinceFull = 0;
  bool endedNotice = false;   // "phase over" line until the next press
  bool forceFull = false;

  void layout(std::vector<Button>& out) const;
  void activate(Id id);
  void saveSettings();
  int minutesShown() const;

 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
      : Activity("Pomodoro", renderer, mappedInput), filePath(std::move(path)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // The countdown must not be cut short by the sleep timeout.
  bool preventAutoSleep() override { return pomo.running(); }
};

// The "Apps" screen opened from the home screen: the to-do lists and the small apps.
class AppsMenuActivity final : public Activity {
  struct Entry {
    const char* label;
    const char* hint;
    int action;  // 0 = to-do lists, 1 = calculator, 2 = pomodoro
  };
  std::vector<Entry> entries;
  int selected = 0;

  void rowRect(int index, int& x, int& y, int& w, int& h) const;
  void open(const Entry& entry);

 public:
  AppsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Apps", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

namespace capps {
// Shows the Apps screen (replaces the current screen).
void openAppsMenu(GfxRenderer& renderer, MappedInputManager& mappedInput);
// Creates /Apps with the two start files the first time (like the starter to-do list).
void ensureStarterApps();
}  // namespace capps
