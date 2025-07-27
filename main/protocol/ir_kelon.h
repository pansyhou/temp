// Copyright 2021 Davide Depau
#include <functional>

/// @file
/// @brief Support for Kelan AC protocol.
/// @note Both sending and decoding should be functional for models of series
/// KELON ON/OFF 9000-12000.
/// All features of the standard remote are implemented.
///
/// @note Unsupported:
///    - Explicit on/off due to AC unit limitations
///    - Explicit swing position due to AC unit limitations
///    - Fahrenheit.
///
/// For KELON168:
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1745

// Supports:
//   Brand: Kelon,  Model: ON/OFF 9000-12000 (KELON)
//   Brand: Kelon,  Model: DG11R2-01 remote (KELON168)
//   Brand: Kelon,  Model: AST-09UW4RVETG00A A/C (KELON168)
//   Brand: Hisense,  Model: AST-09UW4RVETG00A A/C (KELON168)
#ifndef IR_KELON
#define IR_KELON

#include <stdint.h>
#include <functional>

union KelonProtocol {
  uint64_t raw;

  struct {
    uint8_t preamble[2];
    uint8_t Fan: 2;
    uint8_t PowerToggle: 1;
    uint8_t SleepEnabled: 1;
    uint8_t DehumidifierGrade: 3;
    uint8_t SwingVToggle: 1;
    uint8_t Mode: 3;
    uint8_t TimerEnabled: 1;
    uint8_t Temperature: 4;
    uint8_t TimerHalfHour: 1;//定时器
    uint8_t TimerHours: 6;
    uint8_t SmartModeEnabled: 1;
    uint8_t pad1: 4;
    uint8_t SuperCoolEnabled1: 1;
    uint8_t pad2: 2;
    uint8_t SuperCoolEnabled2: 1;
  };
};

// Constants
const uint8_t kKelonModeHeat = 0;
const uint8_t kKelonModeSmart = 1;  // (temp = 26C, but not shown)
const uint8_t kKelonModeCool = 2;
const uint8_t kKelonModeDry = 3;    // (temp = 25C, but not shown)
const uint8_t kKelonModeFan = 4;    // (temp = 25C, but not shown)
const uint8_t kKelonFanAuto = 0;
// Note! Kelon fan speeds are actually 0:AUTO, 1:MAX, 2:MED, 3:MIN
// Since this is insane, I decided to invert them in the public API, they are
// converted back in setFan/getFan
const uint8_t kKelonFanMin = 1;
const uint8_t kKelonFanMedium = 2;
const uint8_t kKelonFanMax = 3;

const int8_t kKelonDryGradeMin = -2;
const int8_t kKelonDryGradeMax = +2;
const uint8_t kKelonMinTemp = 18;
const uint8_t kKelonMaxTemp = 32;
const size_t kKelonFrameSize = 6;//48bits

void make_kelon_frame(const void *primary_data, uint8_t *frame, size_t frame_size);

class IRKelonAc {
 public:
  IRKelonAc();
  ~IRKelonAc();
  void stateReset(void);
  #if SEND_KELON
  void send(const uint16_t repeat = kNoRepeat);
  /// Run the calibration to calculate uSec timing offsets for this platform.
  /// @return The uSec timing offset needed per modulation of the IR Led.
  /// @note This will produce a 65ms IR signal pulse at 38kHz.
  ///   Only ever needs to be run once per object instantiation, if at all.
  int8_t calibrate(void) { return _irsend.calibrate(); }
  /// Since the AC does not support actually setting the power state to a known
  /// value, this utility allow ensuring the AC is on or off by exploiting
  /// the fact that the AC, according to the user manual, will always turn on
  /// when setting it to "smart" or "super" mode.
  void ensurePower(const bool on);
  #endif  // SEND_KELON
  // int8_t calibrate(void) { return _irsend.calibrate(); }
  /// Since the AC does not support actually setting the power state to a known
  /// value, this utility allow ensuring the AC is on or off by exploiting
  /// the fact that the AC, according to the user manual, will always turn on
  /// when setting it to "smart" or "super" mode.
  void ensurePower(const bool on);
  void send(void);
bool current_state=false;

  void setTogglePower(const bool toggle);
  bool getTogglePower(void) const;
  void setTemp(const uint8_t degrees);
  uint8_t getTemp(void) const;
  void setFan(const uint8_t speed);
  uint8_t getFan(void) const;
  void setDryGrade(const int8_t grade);
  int8_t getDryGrade(void) const;
  void setMode(const uint8_t mode);
  uint8_t getMode(void) const;
  void setToggleSwingVertical(const bool toggle);
  bool getToggleSwingVertical(void) const;
  void setSleep(const bool on);
  bool getSleep(void) const;
  void setSupercool(const bool on);
  bool getSupercool(void) const;
  void setTimer(const uint16_t mins);
  uint16_t getTimer(void) const;
  void setTimerEnabled(const bool on);
  bool getTimerEnabled(void) const;
  uint64_t getRaw(void) const;
  void setRaw(const uint64_t new_code);
//   static uint8_t convertMode(const stdAc::opmode_t mode);
//   static uint8_t convertFan(const stdAc::fanspeed_t fan);
//   static stdAc::opmode_t toCommonMode(const uint8_t mode);
//   static stdAc::fanspeed_t toCommonFanSpeed(const uint8_t speed);
//   stdAc::state_t toCommon(const stdAc::state_t *prev = nullptr) const;
//   String toString(void) const;
  void setSend(std::function<void(uint64_t data)> callback);
  //输出当前状态
  void printState(void);
 private:

  KelonProtocol _;
  
  std::function<void(uint64_t data)> _sendKelon;
  // Used when exiting supercool mode
  uint8_t _previousMode = 0;
  uint8_t _previousTemp = kKelonMinTemp;
  uint8_t _previousFan = kKelonFanAuto;
};



#endif