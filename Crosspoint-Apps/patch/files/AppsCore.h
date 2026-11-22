#pragma once
// Logic of the small apps (added by the Crosspoint-Apps patch): calculator and pomodoro timer.
// Pure C++, no firmware headers, so it is tested on a PC (test/test_core.cpp).

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace capps {

// ------------------------------------------------------------------ calculator

enum class Key : uint8_t {
  D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, Dot,
  Add, Sub, Mul, Div, Eq, Clear, Back, Neg, Percent
};

// A pocket calculator: operations run as they are entered (2 + 3 * 4 = 20), like a desk calculator.
class Calculator {
 public:
  void press(Key k) {
    if (error_ && k != Key::Clear) return;
    if (k <= Key::D9) return digit(static_cast<char>('0' + static_cast<int>(k)));
    switch (k) {
      case Key::Dot: return dot();
      case Key::Add: return op('+');
      case Key::Sub: return op('-');
      case Key::Mul: return op('*');
      case Key::Div: return op('/');
      case Key::Eq: return equals();
      case Key::Clear: return clear();
      case Key::Back: return back();
      case Key::Neg: return negate();
      case Key::Percent: return percent();
      default: return;
    }
  }

  // The big line: what is being typed, or the last result.
  std::string display() const {
    if (error_) return "Error";
    return typing_ ? entry_ : format(shown_);
  }
  // The small line above: the pending operation, e.g. "12 +".
  std::string pending() const {
    if (error_ || pendingOp_ == 0) return "";
    return format(acc_) + " " + opChar(pendingOp_);
  }
  bool hasError() const { return error_; }

  static std::string format(double v) {
    if (std::isnan(v) || std::isinf(v)) return "Error";
    if (v == 0) return "0";  // also -0
    char buf[48];
    const double a = std::fabs(v);
    if (a >= 1e12 || a < 1e-9) {
      snprintf(buf, sizeof(buf), "%.6g", v);
      return buf;
    }
    snprintf(buf, sizeof(buf), "%.9f", v);
    std::string s = buf;
    // 9 decimals, then drop what a 12-digit display cannot hold and the trailing zeros
    const size_t dot = s.find('.');
    size_t keepDecimals = 9;
    const size_t intDigits = dot - (s[0] == '-' ? 1 : 0);
    if (intDigits + keepDecimals > 12) keepDecimals = intDigits >= 12 ? 0 : 12 - intDigits;
    snprintf(buf, sizeof(buf), "%.*f", static_cast<int>(keepDecimals), v);
    s = buf;
    if (s.find('.') != std::string::npos) {
      while (s.back() == '0') s.pop_back();
      if (s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
  }

 private:
  std::string entry_ = "0";
  bool typing_ = true;     // entry_ is what is on the display (otherwise shown_)
  double shown_ = 0;       // last result
  double acc_ = 0;         // left operand of the pending operation
  char pendingOp_ = 0;
  bool error_ = false;

  static const char* opChar(char o) {
    switch (o) {
      case '+': return "+";
      case '-': return "-";
      case '*': return "x";
      case '/': return "/";
      default: return "";
    }
  }
  double value() const { return typing_ ? std::strtod(entry_.c_str(), nullptr) : shown_; }

  void digit(char c) {
    if (!typing_ || entry_ == "0" || entry_ == "-0") {
      const bool neg = typing_ && entry_ == "-0";
      entry_ = neg ? "-" : "";
      typing_ = true;
    }
    size_t digits = 0;
    for (char e : entry_) digits += (e >= '0' && e <= '9');
    if (digits >= 12) return;
    entry_ += c;
  }
  void dot() {
    if (!typing_) {
      entry_ = "0";
      typing_ = true;
    }
    if (entry_.find('.') != std::string::npos) return;
    entry_ += ".";
  }
  void back() {
    if (!typing_) return;
    entry_.pop_back();
    if (entry_.empty() || entry_ == "-") entry_ = "0";
  }
  void negate() {
    if (typing_) {
      if (entry_[0] == '-') entry_.erase(0, 1);
      else entry_.insert(0, "-");
    } else {
      shown_ = -shown_;
    }
  }
  void percent() {
    // 50 + 10 %  -> 10 % of 50; alone: value / 100
    double v = value();
    if (pendingOp_ == '+' || pendingOp_ == '-') v = acc_ * v / 100.0;
    else v = v / 100.0;
    setShown(v);
  }
  void setShown(double v) {
    if (std::isnan(v) || std::isinf(v)) {
      error_ = true;
      return;
    }
    shown_ = v;
    typing_ = false;
  }
  bool apply() {
    // pending op with the current value
    const double r = value();
    double out = r;
    if (pendingOp_ != 0) {
      switch (pendingOp_) {
        case '+': out = acc_ + r; break;
        case '-': out = acc_ - r; break;
        case '*': out = acc_ * r; break;
        case '/':
          if (r == 0) {
            error_ = true;
            return false;
          }
          out = acc_ / r;
          break;
      }
    }
    setShown(out);
    return !error_;
  }
  void op(char o) {
    // two operators in a row: the second replaces the first
    if (!typing_ && pendingOp_ != 0 && acc_ == shown_) {
      pendingOp_ = o;
      return;
    }
    if (!apply()) return;
    acc_ = shown_;
    pendingOp_ = o;
  }
  void equals() {
    if (pendingOp_ == 0) {
      setShown(value());
      return;
    }
    if (!apply()) return;
    pendingOp_ = 0;
    acc_ = shown_;
  }
  void clear() {
    entry_ = "0";
    typing_ = true;
    shown_ = acc_ = 0;
    pendingOp_ = 0;
    error_ = false;
  }
};

// ------------------------------------------------------------------ pomodoro timer

enum class Phase : uint8_t { Work, ShortBreak, LongBreak };

struct PomoSettings {
  int workMin = 25;
  int shortMin = 5;
  int longMin = 15;
  int roundsBeforeLong = 4;

  static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
  void sanitize() {
    workMin = clampInt(workMin, 5, 90);
    shortMin = clampInt(shortMin, 1, 30);
    longMin = clampInt(longMin, 5, 60);
    roundsBeforeLong = clampInt(roundsBeforeLong, 2, 8);
  }
  // File content: "work=25 short=5 long=15 rounds=4"
  std::string serialize() const {
    char buf[96];
    snprintf(buf, sizeof(buf), "work=%d short=%d long=%d rounds=%d\n", workMin, shortMin, longMin, roundsBeforeLong);
    return buf;
  }
  void parse(const std::string& text) {
    const auto read = [&](const char* key, int& out) {
      const size_t p = text.find(key);
      if (p == std::string::npos) return;
      out = std::atoi(text.c_str() + p + std::string(key).size());
    };
    read("work=", workMin);
    read("short=", shortMin);
    read("long=", longMin);
    read("rounds=", roundsBeforeLong);
    sanitize();
  }
};

// Counts down one phase at a time. Time comes from the caller (millis), so it can be tested.
class Pomodoro {
 public:
  PomoSettings settings;

  void begin() { enter(Phase::Work); }

  Phase phase() const { return phase_; }
  bool running() const { return running_; }
  int roundsDone() const { return roundsDone_; }  // work phases finished since the long break
  bool phaseJustEnded() const { return justEnded_; }
  void clearJustEnded() { justEnded_ = false; }

  int phaseSeconds() const {
    switch (phase_) {
      case Phase::Work: return settings.workMin * 60;
      case Phase::ShortBreak: return settings.shortMin * 60;
      default: return settings.longMin * 60;
    }
  }

  int remainingSeconds(uint32_t nowMs) const {
    if (!running_) return remaining_;
    const uint32_t elapsed = nowMs - startedMs_;  // wraps correctly
    const int left = remainingAtStart_ - static_cast<int>(elapsed / 1000);
    return left < 0 ? 0 : left;
  }

  void start(uint32_t nowMs) {
    if (running_) return;
    justEnded_ = false;
    if (remaining_ <= 0) remaining_ = phaseSeconds();
    remainingAtStart_ = remaining_;
    startedMs_ = nowMs;
    running_ = true;
  }
  void pause(uint32_t nowMs) {
    if (!running_) return;
    remaining_ = remainingSeconds(nowMs);
    running_ = false;
  }
  void toggle(uint32_t nowMs) { running_ ? pause(nowMs) : start(nowMs); }

  // Restart the current phase from its full length (paused).
  void reset() {
    running_ = false;
    remaining_ = phaseSeconds();
    justEnded_ = false;
  }
  // Skip to the next phase (paused), as if this one had ended.
  void skip() {
    advance();
    justEnded_ = false;
  }

  // Call often. Returns true once, when the running phase has just ended; the next phase is
  // then ready and paused, so the person decides when to go on.
  bool update(uint32_t nowMs) {
    if (!running_) return false;
    if (remainingSeconds(nowMs) > 0) return false;
    advance();
    justEnded_ = true;
    return true;
  }

  // After changing the settings: a paused phase takes its new length.
  void applySettings() {
    settings.sanitize();
    if (!running_) remaining_ = phaseSeconds();
  }

  static std::string clock(int seconds) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", seconds / 60, seconds % 60);
    return buf;
  }

 private:
  Phase phase_ = Phase::Work;
  bool running_ = false;
  bool justEnded_ = false;
  int remaining_ = 25 * 60;
  int remainingAtStart_ = 0;
  uint32_t startedMs_ = 0;
  int roundsDone_ = 0;

  void enter(Phase p) {
    phase_ = p;
    running_ = false;
    remaining_ = phaseSeconds();
  }
  void advance() {
    running_ = false;
    if (phase_ == Phase::Work) {
      ++roundsDone_;
      if (roundsDone_ >= settings.roundsBeforeLong) {
        roundsDone_ = 0;
        enter(Phase::LongBreak);
      } else {
        enter(Phase::ShortBreak);
      }
    } else {
      enter(Phase::Work);
    }
  }
};

// ------------------------------------------------------------------ app files

// The apps start from small files in /Apps: tap "Calculator" or "Pomodoro" in the file browser.
constexpr const char* kAppsDir = "/Apps";
constexpr const char* kCalculatorFile = "/Apps/Calculator.txt";
constexpr const char* kPomodoroFile = "/Apps/Pomodoro.txt";

enum class AppKind : uint8_t { None, Calculator, Pomodoro };

inline AppKind appKind(const std::string& path) {
  const auto same = [&](const char* name) {
    const std::string n(name);
    if (path.size() != n.size()) return false;
    for (size_t i = 0; i < n.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(path[i])) != std::tolower(static_cast<unsigned char>(n[i]))) return false;
    }
    return true;
  };
  if (same(kCalculatorFile)) return AppKind::Calculator;
  if (same(kPomodoroFile)) return AppKind::Pomodoro;
  return AppKind::None;
}

}  // namespace capps
