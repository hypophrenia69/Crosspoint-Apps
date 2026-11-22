#include "AppsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {

using Btn = MappedInputManager::Button;

// Text centred on (cx, cy).
void centered(const GfxRenderer& r, int font, int cx, int cy, const char* text, bool black = true) {
  const int w = r.getTextWidth(font, text);
  const int h = r.getLineHeight(font);
  r.drawText(font, cx - w / 2, cy - h / 2, text, black);
}

void rightAligned(const GfxRenderer& r, int font, int right, int y, const char* text) {
  r.drawText(font, right - r.getTextWidth(font, text), y, text);
}

// A key: outlined, or filled black with white text when it carries the frame (button devices).
void drawKey(const GfxRenderer& r, int x, int y, int w, int h, const char* label, bool focused, int font) {
  if (focused) {
    r.fillRect(x, y, w, h, true);
    centered(r, font, x + w / 2, y + h / 2, label, false);
  } else {
    r.drawRect(x, y, w, h, 2, true);
    centered(r, font, x + w / 2, y + h / 2, label, true);
  }
}

bool inside(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

// Seven-segment digit for the timer: big and crisp on e-ink.
// Segments: 0 top, 1 top-right, 2 bottom-right, 3 bottom, 4 bottom-left, 5 top-left, 6 middle.
constexpr uint8_t SEGMENTS[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
                                  0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

void drawDigit(const GfxRenderer& r, int digit, int x, int y, int w, int h, int t) {
  if (digit < 0 || digit > 9) return;
  const uint8_t m = SEGMENTS[digit];
  const int hh = h / 2;
  if (m & (1 << 0)) r.fillRect(x + t, y, w - 2 * t, t, true);
  if (m & (1 << 1)) r.fillRect(x + w - t, y + t, t, hh - t - t / 2, true);
  if (m & (1 << 2)) r.fillRect(x + w - t, y + hh + t / 2, t, hh - t - t / 2, true);
  if (m & (1 << 3)) r.fillRect(x + t, y + h - t, w - 2 * t, t, true);
  if (m & (1 << 4)) r.fillRect(x, y + hh + t / 2, t, hh - t - t / 2, true);
  if (m & (1 << 5)) r.fillRect(x, y + t, t, hh - t - t / 2, true);
  if (m & (1 << 6)) r.fillRect(x + t, y + hh - t / 2, w - 2 * t, t, true);
}

// Whole screen refresh now and then, so the ghosts of earlier digits do not pile up.
void showFrame(const GfxRenderer& r, int& updatesSinceFull) {
  if (++updatesSinceFull >= 8) {
    updatesSinceFull = 0;
    r.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    r.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

// ---------------------------------------------------------------- calculator layout

struct KeyDef {
  const char* label;
  capps::Key key;
};
constexpr int CALC_COLS = 4;
constexpr int CALC_ROWS = 5;
constexpr KeyDef CALC_KEYS[CALC_COLS * CALC_ROWS] = {
    {"C", capps::Key::Clear}, {"<", capps::Key::Back},  {"%", capps::Key::Percent}, {"/", capps::Key::Div},
    {"7", capps::Key::D7},    {"8", capps::Key::D8},    {"9", capps::Key::D9},      {"x", capps::Key::Mul},
    {"4", capps::Key::D4},    {"5", capps::Key::D5},    {"6", capps::Key::D6},      {"-", capps::Key::Sub},
    {"1", capps::Key::D1},    {"2", capps::Key::D2},    {"3", capps::Key::D3},      {"+", capps::Key::Add},
    {"+/-", capps::Key::Neg}, {"0", capps::Key::D0},    {".", capps::Key::Dot},     {"=", capps::Key::Eq},
};

struct Grid {
  int x, y, cellW, cellH, gap;
};
Grid calcGrid(const GfxRenderer& r) {
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const int margin = 16;
  const int gap = 8;
  const int top = 190;
  const int bottom = h - 70;  // room for the button hints
  Grid g;
  g.gap = gap;
  g.x = margin;
  g.y = top;
  g.cellW = (w - 2 * margin - (CALC_COLS - 1) * gap) / CALC_COLS;
  g.cellH = (bottom - top - (CALC_ROWS - 1) * gap) / CALC_ROWS;
  return g;
}

}  // namespace

// ==================================================================== calculator

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void CalculatorActivity::loop() {
  if (mappedInput.wasReleased(Btn::Back)) {
    capps::openAppsMenu(renderer, mappedInput);
    return;
  }

  bool changed = false;
  int tx = 0;
  int ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const Grid g = calcGrid(renderer);
    for (int i = 0; i < CALC_COLS * CALC_ROWS; ++i) {
      const int x = g.x + (i % CALC_COLS) * (g.cellW + g.gap);
      const int y = g.y + (i / CALC_COLS) * (g.cellH + g.gap);
      if (inside(tx, ty, x, y, g.cellW, g.cellH)) {
        RenderLock lock(*this);
        selected = i;
        calc.press(CALC_KEYS[i].key);
        changed = true;
        break;
      }
    }
  } else if (mappedInput.wasPressed(Btn::Left)) {
    selected = (selected % CALC_COLS == 0) ? selected + CALC_COLS - 1 : selected - 1;
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Right)) {
    selected = (selected % CALC_COLS == CALC_COLS - 1) ? selected - (CALC_COLS - 1) : selected + 1;
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Up)) {
    selected = (selected + CALC_COLS * CALC_ROWS - CALC_COLS) % (CALC_COLS * CALC_ROWS);
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Down)) {
    selected = (selected + CALC_COLS) % (CALC_COLS * CALC_ROWS);
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Confirm)) {
    RenderLock lock(*this);
    calc.press(CALC_KEYS[selected].key);
    changed = true;
  }
  if (changed) requestUpdate();
}

void CalculatorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int w = renderer.getScreenWidth();
  const int margin = 16;

  renderer.drawText(UI_10_FONT_ID, margin, 14, "Calculator");

  // Display: the pending operation small, the number big.
  const std::string pending = calc.pending();
  if (!pending.empty()) rightAligned(renderer, UI_10_FONT_ID, w - margin, 70, pending.c_str());
  const std::string shown = calc.display();
  rightAligned(renderer, UI_12_FONT_ID, w - margin, 105, shown.c_str());
  renderer.drawLine(margin, 165, w - margin, 165, 2, true);

  const Grid g = calcGrid(renderer);
  const bool showFrame_ = !mappedInput.hasTouch();  // the key frame only helps on button devices
  for (int i = 0; i < CALC_COLS * CALC_ROWS; ++i) {
    const int x = g.x + (i % CALC_COLS) * (g.cellW + g.gap);
    const int y = g.y + (i / CALC_COLS) * (g.cellH + g.gap);
    drawKey(renderer, x, y, g.cellW, g.cellH, CALC_KEYS[i].label, showFrame_ && i == selected, UI_12_FONT_ID);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  showFrame(renderer, updatesSinceFull);
}

// ==================================================================== pomodoro

int PomodoroActivity::minutesShown() const {
  const int s = pomo.remainingSeconds(millis());
  return (s + 59) / 60;  // whole minutes, rounded up: 0:01 still shows 1
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  pomo.begin();
  const String saved = Storage.readFile(filePath.c_str());
  pomo.settings.parse(std::string(saved.c_str()));
  pomo.applySettings();
  lastMinutesShown = minutesShown();
  requestUpdate();
}

void PomodoroActivity::saveSettings() {
  const std::string text = pomo.settings.serialize();
  Storage.writeFile(filePath.c_str(), String(text.c_str()));
}

void PomodoroActivity::onExit() {
  saveSettings();
  Activity::onExit();
}

void PomodoroActivity::layout(std::vector<Button>& out) const {
  out.clear();
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int margin = 16;
  const int gap = 10;
  // Start/Pause, Skip, Reset in one row.
  const int rowY = 400;
  const int rowH = 72;
  const int bigW = (w - 2 * margin - 2 * gap) / 2;
  const int smallW = (bigW - gap) / 2;
  out.push_back({margin, rowY, bigW, rowH, Id::StartPause, pomo.running() ? "Pause" : "Start"});
  out.push_back({margin + bigW + gap, rowY, smallW, rowH, Id::Skip, "Skip"});
  out.push_back({margin + bigW + gap + smallW + gap, rowY, smallW, rowH, Id::Reset, "Reset"});

  // Four setting rows: label (drawn in render), then [-] value [+].
  const int firstY = 520;
  const int rowStep = std::min(64, (h - 70 - firstY) / 4);
  const int boxH = rowStep - 8;
  const int boxW = 76;
  const Id minus[4] = {Id::WorkMinus, Id::ShortMinus, Id::LongMinus, Id::RoundsMinus};
  const Id plus[4] = {Id::WorkPlus, Id::ShortPlus, Id::LongPlus, Id::RoundsPlus};
  for (int i = 0; i < 4; ++i) {
    const int y = firstY + i * rowStep;
    out.push_back({w - margin - boxW * 2 - 150, y, boxW, boxH, minus[i], "-"});
    out.push_back({w - margin - boxW, y, boxW, boxH, plus[i], "+"});
  }
}

void PomodoroActivity::activate(Id id) {
  const uint32_t now = millis();
  auto& s = pomo.settings;
  endedNotice = false;
  switch (id) {
    case Id::StartPause: pomo.toggle(now); break;
    case Id::Skip: pomo.skip(); forceFull = true; break;
    case Id::Reset: pomo.reset(); break;
    case Id::WorkMinus: s.workMin -= 5; break;
    case Id::WorkPlus: s.workMin += 5; break;
    case Id::ShortMinus: s.shortMin -= 1; break;
    case Id::ShortPlus: s.shortMin += 1; break;
    case Id::LongMinus: s.longMin -= 5; break;
    case Id::LongPlus: s.longMin += 5; break;
    case Id::RoundsMinus: s.roundsBeforeLong -= 1; break;
    case Id::RoundsPlus: s.roundsBeforeLong += 1; break;
  }
  pomo.applySettings();
  lastMinutesShown = minutesShown();
}

void PomodoroActivity::loop() {
  if (mappedInput.wasReleased(Btn::Back)) {
    capps::openAppsMenu(renderer, mappedInput);
    return;
  }

  bool changed = false;

  // The phase ran out: the next one is ready and waits for a press.
  if (pomo.running()) {
    RenderLock lock(*this);
    if (pomo.update(millis())) {
      endedNotice = true;
      forceFull = true;
      lastMinutesShown = minutesShown();
      changed = true;
    } else if (pomo.running() && minutesShown() != lastMinutesShown) {
      lastMinutesShown = minutesShown();
      changed = true;
    }
  }

  std::vector<Button> buttons;
  layout(buttons);
  const int count = static_cast<int>(buttons.size());
  int tx = 0;
  int ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    for (int i = 0; i < count; ++i) {
      if (inside(tx, ty, buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h)) {
        RenderLock lock(*this);
        selected = i;
        activate(buttons[i].id);
        changed = true;
        break;
      }
    }
  } else if (mappedInput.wasPressed(Btn::Left) || mappedInput.wasPressed(Btn::Up)) {
    selected = (selected + count - 1) % count;
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Right) || mappedInput.wasPressed(Btn::Down)) {
    selected = (selected + 1) % count;
    changed = true;
  } else if (mappedInput.wasPressed(Btn::Confirm)) {
    RenderLock lock(*this);
    activate(buttons[selected].id);
    changed = true;
  }
  if (changed) requestUpdate();
}

void PomodoroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int w = renderer.getScreenWidth();
  const int margin = 16;
  const auto& s = pomo.settings;

  renderer.drawText(UI_10_FONT_ID, margin, 14, "Pomodoro");

  const char* phaseName = pomo.phase() == capps::Phase::Work         ? "Focus"
                          : pomo.phase() == capps::Phase::ShortBreak ? "Short break"
                                                                     : "Long break";
  centered(renderer, UI_12_FONT_ID, w / 2, 90, phaseName);

  // Remaining minutes as big seven-segment digits.
  const int minutes = std::min(minutesShown(), 99);
  const int digitW = 110;
  const int digitH = 190;
  const int thick = 22;
  const int gapD = 24;
  const int totalW = digitW * 2 + gapD;
  const int x0 = (w - totalW) / 2;
  const int y0 = 130;
  drawDigit(renderer, minutes / 10, x0, y0, digitW, digitH, thick);
  drawDigit(renderer, minutes % 10, x0 + digitW + gapD, y0, digitW, digitH, thick);
  centered(renderer, UI_10_FONT_ID, w / 2, y0 + digitH + 24, pomo.running() ? "minutes left" : "minutes");

  // Rounds until the long break.
  const int dots = s.roundsBeforeLong;
  const int dotSize = 22;
  const int dotGap = 14;
  const int dotsW = dots * dotSize + (dots - 1) * dotGap;
  for (int i = 0; i < dots; ++i) {
    const int dx = (w - dotsW) / 2 + i * (dotSize + dotGap);
    if (i < pomo.roundsDone()) renderer.fillRect(dx, 362, dotSize, dotSize, true);
    else renderer.drawRect(dx, 362, dotSize, dotSize, 2, true);
  }

  if (endedNotice) centered(renderer, UI_10_FONT_ID, w / 2, 392, "Time is up - press Start for the next one");

  std::vector<Button> buttons;
  layout(buttons);
  const bool frame = !mappedInput.hasTouch();
  for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
    const Button& b = buttons[i];
    drawKey(renderer, b.x, b.y, b.w, b.h, b.label, frame && i == selected, UI_12_FONT_ID);
  }

  // Setting labels and values, left of the [-] [+] buttons.
  const char* names[4] = {"Focus (min)", "Short break", "Long break", "Rounds"};
  const int values[4] = {s.workMin, s.shortMin, s.longMin, s.roundsBeforeLong};
  for (int i = 0; i < 4; ++i) {
    const Button& minus = buttons[3 + i * 2];
    const int midY = minus.y + minus.h / 2;
    renderer.drawText(UI_10_FONT_ID, margin, midY - renderer.getLineHeight(UI_10_FONT_ID) / 2, names[i]);
    char num[8];
    snprintf(num, sizeof(num), "%d", values[i]);
    const Button& plus = buttons[4 + i * 2];
    centered(renderer, UI_12_FONT_ID, (minus.x + minus.w + plus.x) / 2, midY, num);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "OK", "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (forceFull) {
    forceFull = false;
    updatesSinceFull = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    showFrame(renderer, updatesSinceFull);
  }
}

// ==================================================================== apps screen

void AppsMenuActivity::onEnter() {
  Activity::onEnter();
  entries.clear();
  // The to-do lists come from the Crosspoint-ToDoList add-on: shown when its folder exists.
  if (Storage.exists("/Lists")) entries.push_back({"To-do lists", "your lists", 0});
  entries.push_back({"Calculator", "+ - x /", 1});
  entries.push_back({"Pomodoro", "focus timer", 2});
  selected = 0;
  requestUpdate();
}

void AppsMenuActivity::rowRect(int index, int& x, int& y, int& w, int& h) const {
  const int margin = 16;
  x = margin;
  w = renderer.getScreenWidth() - 2 * margin;
  h = 92;
  y = 110 + index * (h + 12);
}

void AppsMenuActivity::open(const Entry& entry) {
  switch (entry.action) {
    case 0: activityManager.goToFileBrowser("/Lists"); break;
    case 1: activityManager.goToReader(capps::kCalculatorFile); break;
    default: activityManager.goToReader(capps::kPomodoroFile); break;
  }
}

void AppsMenuActivity::loop() {
  if (mappedInput.wasReleased(Btn::Back)) {
    activityManager.goHome(HomeMenuItem::APPS);
    return;
  }
  const int count = static_cast<int>(entries.size());
  int tx = 0;
  int ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    for (int i = 0; i < count; ++i) {
      int x, y, w, h;
      rowRect(i, x, y, w, h);
      if (inside(tx, ty, x, y, w, h)) {
        open(entries[i]);
        return;
      }
    }
  } else if (mappedInput.wasPressed(Btn::Up) || mappedInput.wasPressed(Btn::Left)) {
    selected = (selected + count - 1) % count;
    requestUpdate();
  } else if (mappedInput.wasPressed(Btn::Down) || mappedInput.wasPressed(Btn::Right)) {
    selected = (selected + 1) % count;
    requestUpdate();
  } else if (mappedInput.wasPressed(Btn::Confirm)) {
    open(entries[selected]);
  }
}

void AppsMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int w = renderer.getScreenWidth();
  renderer.drawText(UI_12_FONT_ID, 16, 40, "Apps");
  renderer.drawLine(16, 92, w - 16, 92, 2, true);
  const bool frame = !mappedInput.hasTouch();
  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    int x, y, rw, rh;
    rowRect(i, x, y, rw, rh);
    const bool focused = frame && i == selected;
    if (focused) renderer.fillRect(x, y, rw, rh, true);
    else renderer.drawRect(x, y, rw, rh, 2, true);
    const int textH = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawText(UI_12_FONT_ID, x + 20, y + rh / 2 - textH / 2, entries[i].label, !focused);
    const int hintW = renderer.getTextWidth(UI_10_FONT_ID, entries[i].hint);
    renderer.drawText(UI_10_FONT_ID, x + rw - 20 - hintW, y + rh / 2 - renderer.getLineHeight(UI_10_FONT_ID) / 2,
                      entries[i].hint, !focused);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), frame ? "OK" : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// ==================================================================== starter files

namespace capps {

void openAppsMenu(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  auto menu = makeUniqueNoThrow<AppsMenuActivity>(renderer, mappedInput);
  if (!menu) {
    LOG_ERR("APPS", "OOM: apps screen");
    return;
  }
  activityManager.replaceActivity(std::move(menu));
}

void ensureStarterApps() {
  if (!Storage.exists(kCalculatorFile) || !Storage.exists(kPomodoroFile)) {
    Storage.ensureDirectoryExists(kAppsDir);
  }
  if (!Storage.exists(kCalculatorFile)) {
    Storage.writeFile(kCalculatorFile, String("Calculator. Open this file in the file browser to start it.\n"));
  }
  if (!Storage.exists(kPomodoroFile)) {
    Storage.writeFile(kPomodoroFile, String(PomoSettings().serialize().c_str()));
  }
}

}  // namespace capps
