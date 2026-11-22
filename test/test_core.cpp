#include <cassert>
#include <cstdio>
#include "../patch/files/AppsCore.h"
using namespace capps;

static void keys(Calculator& c, const char* s) {
  for (; *s; ++s) {
    switch (*s) {
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        c.press(static_cast<Key>(*s - '0')); break;
      case '.': c.press(Key::Dot); break;
      case '+': c.press(Key::Add); break;
      case '-': c.press(Key::Sub); break;
      case '*': c.press(Key::Mul); break;
      case '/': c.press(Key::Div); break;
      case '=': c.press(Key::Eq); break;
      case 'C': c.press(Key::Clear); break;
      case 'B': c.press(Key::Back); break;
      case 'N': c.press(Key::Neg); break;
      case '%': c.press(Key::Percent); break;
    }
  }
}
static std::string calc(const char* s) { Calculator c; keys(c, s); return c.display(); }

int main() {
  assert(calc("2+3=") == "5");
  assert(calc("2+3*4=") == "20");               // immediate execution
  assert(calc("12/4=") == "3");
  assert(calc("1/3=") == "0.333333333");
  assert(calc("0.1+0.2=") == "0.3");
  assert(calc("5/0=") == "Error");
  assert(calc("5/0C7") == "7");                 // clear recovers from the error
  assert(calc("7B") == "0");
  assert(calc("123B") == "12");
  assert(calc("2.5*2=") == "5");
  assert(calc("5N") == "-5");
  assert(calc("5N+2=") == "-3");
  assert(calc("50+10%=") == "55");               // percent of the left operand
  assert(calc("200%") == "2");
  assert(calc("9-") == "9");
  assert(calc("9+-2=") == "7");                  // second operator replaces the first
  assert(calc("3=") == "3");
  assert(calc("3+=") == "6");                    // 3 + 3
  assert(calc("1..5") == "1.5");                 // a second dot is ignored
  assert(calc("999999999999*10=") == "1e+13");  // too big for 12 digits: scientific
  assert(calc("0000") == "0");
  assert(calc("2=+3=") == "5");                  // result goes on
  { Calculator c; keys(c, "12+"); assert(c.pending() == "12 +"); }
  assert(calc("1000000*1000000=") == "1e+12");
  printf("calculator OK\n");

  // pomodoro
  Pomodoro p; p.begin();
  assert(p.phase() == Phase::Work && !p.running());
  assert(p.remainingSeconds(0) == 25 * 60);
  p.start(1000);
  assert(p.remainingSeconds(1000 + 61 * 1000) == 25 * 60 - 61);
  p.pause(1000 + 61000);
  assert(p.remainingSeconds(99999999) == 25 * 60 - 61);   // paused stays
  p.start(5000);
  assert(!p.update(5000 + 1000));
  assert(p.update(5000 + (25 * 60 - 61) * 1000 + 1));      // phase ends
  assert(p.phase() == Phase::ShortBreak && !p.running() && p.phaseJustEnded());
  assert(p.remainingSeconds(0) == 5 * 60);
  // run through a full cycle: 4 work phases -> long break
  p.skip();  // break -> work
  for (int round = 2; round <= 4; ++round) {
    assert(p.phase() == Phase::Work);
    p.skip();                                  // work -> break (round counted)
    if (round < 4) { assert(p.phase() == Phase::ShortBreak); p.skip(); }
  }
  assert(p.phase() == Phase::LongBreak && p.roundsDone() == 0);
  p.skip();
  assert(p.phase() == Phase::Work);
  // millis wrap-around
  Pomodoro w; w.begin(); w.start(0xFFFFFF00u);
  assert(w.remainingSeconds(0xFFFFFF00u + 3000) == 25 * 60 - 3);
  assert(w.remainingSeconds(100) == 25 * 60 - 0);  // 356 ms later: still 25:00
  // settings
  PomoSettings s; s.parse("work=50 short=10 long=30 rounds=3");
  assert(s.workMin == 50 && s.shortMin == 10 && s.longMin == 30 && s.roundsBeforeLong == 3);
  s.parse("work=9999 short=-4");
  assert(s.workMin == 90 && s.shortMin == 1);
  PomoSettings d; d.parse("garbage");
  assert(d.workMin == 25);
  assert(Pomodoro::clock(65) == "01:05" && Pomodoro::clock(25 * 60) == "25:00");
  Pomodoro q; q.begin(); q.settings.workMin = 40; q.applySettings();
  assert(q.remainingSeconds(0) == 40 * 60);
  assert(appKind("/Apps/Calculator.txt") == AppKind::Calculator);
  assert(appKind("/apps/pomodoro.txt") == AppKind::Pomodoro);
  assert(appKind("/Books/Calculator.txt") == AppKind::None && appKind("/Apps/Other.txt") == AppKind::None);
  printf("pomodoro OK\nALL OK\n");
}
