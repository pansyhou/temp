#include "ir_kelon.h"
#include "esp_log.h"
#include <algorithm>

const char *TAG = "IRKelonAc";

// Constants
const uint16_t kKelonHdrMark = 9000;
const uint16_t kKelonHdrSpace = 4600;
const uint16_t kKelonBitMark =   560;
const uint16_t kKelonOneSpace = 1680;
const uint16_t kKelonZeroSpace = 600;
// const uint32_t kKelonGap = 2 * kDefaultMessageGap;
const uint16_t kKelonFreq = 38000;

const uint32_t kKelon168FooterSpace = 8000;
const uint16_t kKelon168Section1Size = 6;
const uint16_t kKelon168Section2Size = 8;
const uint16_t kKelon168Section3Size = 7;


//发送函数原型
// esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_t *encoder, const void *payload, size_t payload_bytes, const rmt_transmit_config_t *config)


void make_kelon_frame(const void *primary_data, uint8_t *frame, size_t frame_size){
    if(primary_data==nullptr && frame==nullptr){
        return;
    }
    //LSB uint64_t
    for(int i = 0; i < kKelonFrameSize; i++){
        frame[i] = *((uint8_t *)primary_data) & 0xFF;
        primary_data = (uint8_t *)primary_data + 1;
    }
}

//构造
IRKelonAc::IRKelonAc(){
    stateReset();
}

//析构
IRKelonAc::~IRKelonAc(){
    stateReset();
    //delet
}

void IRKelonAc::stateReset() {
  _.raw = 0L;
  _.preamble[0] = 0b10000011;
  _.preamble[1] = 0b00000110;
}

void IRKelonAc::send(void) {
  if(_sendKelon!=nullptr)_sendKelon(getRaw());

  // Reset toggle flags
  _.PowerToggle = false;
  _.SwingVToggle = false;

  // Remove the timer time setting
  _.TimerHours = 0;
  _.TimerHalfHour = 0;
}

void IRKelonAc::setSend(std::function<void(uint64_t data)> callback){
    _sendKelon = callback;
}

/// Ensures the AC is on or off by exploiting the fact that setting
/// it to "smart" will always turn it on if it's off.
/// This method will send 2 commands to the AC to do the trick
/// @param[in] on Whether to ensure the AC is on or off
void IRKelonAc::ensurePower(bool on) {
  // Try to avoid turning on the compressor for this operation.
  // "Dry grade", when in "smart" mode, acts as a temperature offset that
  // the user can configure if they feel too cold or too hot. By setting it
  // to +2 we're setting the temperature to ~28°C, which will effectively
  // set the AC to fan mode.
  int8_t previousDry = getDryGrade();
  setDryGrade(2);
  setMode(kKelonModeSmart);
  send();
  setDryGrade(previousDry);
  setMode(_previousMode);
  send();

  // Now we're sure it's on. Turn it back off. The AC seems to turn back on if
  // we don't send this separately
  if (!on) {
    setTogglePower(true);
    send();
  }
}


///////

/// Request toggling power - will be reset to false after sending
/// @param[in] toggle Whether to toggle the power state
void IRKelonAc::setTogglePower(const bool toggle) {
   _.PowerToggle = toggle; 
    current_state = !current_state;//取反
}

/// Get whether toggling power will be requested
/// @return The power toggle state
bool IRKelonAc::getTogglePower() const { return current_state; }

/// Set the temperature setting.
/// @param[in] degrees The temperature in degrees celsius.
void IRKelonAc::setTemp(const uint8_t degrees) {
  uint8_t temp = std::max(kKelonMinTemp, degrees);
  temp = std::min(kKelonMaxTemp, temp);
  _previousTemp = _.Temperature;
  _.Temperature = temp - kKelonMinTemp;
}

/// Get the current temperature setting.
/// @return Get current setting for temp. in degrees celsius.
uint8_t IRKelonAc::getTemp() const { return _.Temperature + kKelonMinTemp; }

/// Set the speed of the fan.
/// @param[in] speed 0 is auto, 1-5 is the speed
void IRKelonAc::setFan(const uint8_t speed) {
  uint8_t fan = std::min(speed, kKelonFanMax);

  _previousFan = _.Fan;
  // Note: Kelon fan speeds are backwards! This code maps the range 0,1:3 to
  // 0,3:1 to save the API's user's sanity.
  _.Fan = ((static_cast<int16_t>(fan) - 4) * -1) % 4;
}

/// Get the current fan speed setting.
/// @return The current fan speed.
uint8_t IRKelonAc::getFan() const {
  return ((static_cast<int16_t>(_.Fan) - 4) * -1) % 4;;
}

/// Set the dehumidification intensity.
/// @param[in] grade has to be in the range [-2 : +2]
void IRKelonAc::setDryGrade(const int8_t grade) {
  int8_t drygrade = std::max(kKelonDryGradeMin, grade);
  drygrade = std::min(kKelonDryGradeMax, drygrade);

  // Two's complement is clearly too bleeding edge for this manufacturer
  uint8_t outval;
  if (drygrade < 0)
    outval = 0b100 | (-drygrade & 0b011);
  else
    outval = drygrade & 0b011;
  _.DehumidifierGrade = outval;
}

/// Get the current dehumidification intensity setting. In smart mode, this
/// controls the temperature adjustment.
/// @return The current dehumidification intensity.
int8_t IRKelonAc::getDryGrade() const {
  return static_cast<int8_t>(_.DehumidifierGrade & 0b011) *
         ((_.DehumidifierGrade & 0b100) ? -1 : 1);
}

/// Set the desired operation mode.
/// @param[in] mode The desired operation mode.
void IRKelonAc::setMode(const uint8_t mode) {
  if (_.Mode == kKelonModeSmart || _.Mode == kKelonModeFan ||
      _.Mode == kKelonModeDry) {
    _.Temperature = _previousTemp;
  }
  if (_.SuperCoolEnabled1) {
    // Cancel supercool
    _.SuperCoolEnabled1 = false;
    _.SuperCoolEnabled2 = false;
    _.Temperature = _previousTemp;
    _.Fan = _previousFan;
  }
  _previousMode = _.Mode;

  switch (mode) {
    case kKelonModeSmart:
      setTemp(26);
      _.SmartModeEnabled = true;
      _.Mode = mode;
      break;
    case kKelonModeDry:
    case kKelonModeFan:
      setTemp(25);
      // fallthrough
    case kKelonModeCool:
    case kKelonModeHeat:
      _.Mode = mode;
      // fallthrough
    default:
      _.SmartModeEnabled = false;
  }
}

/// Get the current operation mode setting.
/// @return The current operation mode.
uint8_t IRKelonAc::getMode() const { return _.Mode; }

/// Request toggling the vertical swing - will be reset to false after sending
/// @param[in] toggle If true, the swing mode will be toggled when sent.
void IRKelonAc::setToggleSwingVertical(const bool toggle) {
  _.SwingVToggle = toggle;
}

/// Get whether the swing mode is set to be toggled
/// @return Whether the toggle bit is set
bool IRKelonAc::getToggleSwingVertical() const { return _.SwingVToggle; }

/// Control the current sleep (quiet) setting.
/// @param[in] on The desired setting.
void IRKelonAc::setSleep(const bool on) { _.SleepEnabled = on; }

/// Is the sleep setting on?
/// @return The current value.
bool IRKelonAc::getSleep() const { return _.SleepEnabled; }

/// Control the current super cool mode setting.
/// @param[in] on The desired setting.
void IRKelonAc::setSupercool(const bool on) {
  if (on) {
    setTemp(kKelonMinTemp);
    setMode(kKelonModeCool);
    setFan(kKelonFanMax);
  } else {
    // All reverts to previous are handled by setMode as needed
    setMode(_previousMode);
  }
  _.SuperCoolEnabled1 = on;
  _.SuperCoolEnabled2 = on;
}

/// Is the super cool mode setting on?
/// @return The current value.
bool IRKelonAc::getSupercool() const { return _.SuperCoolEnabled1; }

/// Set the timer time and enable it. Timer is an off timer if the unit is on,
/// it is an on timer if the unit is off.
/// Only multiples of 30m are supported for < 10h, then only multiples of 60m
/// @param[in] mins Nr. of minutes
void IRKelonAc::setTimer(uint16_t mins) {
  const uint16_t minutes = std::min(static_cast<int>(mins), 24 * 60);

  if (minutes / 60 >= 10) {
    uint8_t hours = minutes / 60 + 10;
    _.TimerHalfHour = hours & 1;
    _.TimerHours = hours >> 1;
  } else {
    _.TimerHalfHour = (minutes % 60) >= 30 ? 1 : 0;
    _.TimerHours = minutes / 60;
  }

  setTimerEnabled(true);
}

/// Get the set timer. Timer set time is deleted once the command is sent, so
/// calling this after send() will return 0.
/// The AC unit will continue keeping track of the remaining time unless it is
/// later disabled.
/// @return The timer set minutes
uint16_t IRKelonAc::getTimer() const {
  if (_.TimerHours >= 10)
    return (static_cast<uint16_t>((_.TimerHours << 1) | _.TimerHalfHour) -
            10) * 60;
  return static_cast<uint16_t>(_.TimerHours) * 60 + (_.TimerHalfHour ? 30 : 0);
}

/// Enable or disable the timer. Note that in order to enable the timer the
/// minutes must be set with setTimer().
/// @param[in] on Whether to enable or disable the timer
void IRKelonAc::setTimerEnabled(bool on) { _.TimerEnabled = on; }

/// Get the current timer status
/// @return Whether the timer is enabled.
bool IRKelonAc::getTimerEnabled() const { return _.TimerEnabled; }

/// Get the raw state of the object, suitable to be sent with the appropriate
/// IRsend object method.
/// @return A PTR to the internal state.
uint64_t IRKelonAc::getRaw() const { return _.raw; }

/// Set the raw state of the object.
/// @param[in] new_code The raw state from the native IR message.
void IRKelonAc::setRaw(const uint64_t new_code) { _.raw = new_code; }

void IRKelonAc::printState(void){
    //用esplog输出当前状态
    ESP_LOGI(TAG, "当前状态:");
    ESP_LOGI(TAG, "温度: %d", _.Temperature);
    ESP_LOGI(TAG, "模式: %d", _.Mode);
    ESP_LOGI(TAG, "风速: %d", _.Fan);
    ESP_LOGI(TAG, "睡眠: %d", _.SleepEnabled);
    ESP_LOGI(TAG, "智能: %d", _.SmartModeEnabled);
    ESP_LOGI(TAG, "超酷: %d", _.SuperCoolEnabled1);
    ESP_LOGI(TAG, "定时器: %d", _.TimerEnabled);
    ESP_LOGI(TAG, "定时器时间: %d", getTimer());
}


// /// Convert a standard A/C mode (stdAc::opmode_t) into it a native mode.
// /// @param[in] mode A stdAc::opmode_t operation mode.
// /// @return The native mode equivalent.
// uint8_t IRKelonAc::convertMode(const stdAc::opmode_t mode) {
//   switch (mode) {
//     case stdAc::opmode_t::kCool: return kKelonModeCool;
//     case stdAc::opmode_t::kHeat: return kKelonModeHeat;
//     case stdAc::opmode_t::kDry:  return kKelonModeDry;
//     case stdAc::opmode_t::kFan:  return kKelonModeFan;
//     default:                     return kKelonModeSmart;  // aka Auto.
//   }
// }

// /// Convert a standard A/C fan speed (stdAc::fanspeed_t) into it a native speed.
// /// @param[in] fan A stdAc::fanspeed_t fan speed
// /// @return The native speed equivalent.
// uint8_t IRKelonAc::convertFan(stdAc::fanspeed_t fan) {
//   switch (fan) {
//     case stdAc::fanspeed_t::kMin:
//     case stdAc::fanspeed_t::kLow:    return kKelonFanMin;
//     case stdAc::fanspeed_t::kMedium: return kKelonFanMedium;
//     case stdAc::fanspeed_t::kHigh:
//     case stdAc::fanspeed_t::kMax:    return kKelonFanMax;
//     default:                         return kKelonFanAuto;
//   }
// }

// /// Convert a native mode to it's stdAc::opmode_t equivalent.
// /// @param[in] mode A native operating mode value.
// /// @return The stdAc::opmode_t equivalent.
// stdAc::opmode_t IRKelonAc::toCommonMode(const uint8_t mode) {
//   switch (mode) {
//     case kKelonModeCool: return stdAc::opmode_t::kCool;
//     case kKelonModeHeat: return stdAc::opmode_t::kHeat;
//     case kKelonModeDry:  return stdAc::opmode_t::kDry;
//     case kKelonModeFan:  return stdAc::opmode_t::kFan;
//     default:             return stdAc::opmode_t::kAuto;
//   }
// }

// /// Convert a native fan speed to it's stdAc::fanspeed_t equivalent.
// /// @param[in] speed A native fan speed value.
// /// @return The stdAc::fanspeed_t equivalent.
// stdAc::fanspeed_t IRKelonAc::toCommonFanSpeed(const uint8_t speed) {
//   switch (speed) {
//     case kKelonFanMin:    return stdAc::fanspeed_t::kLow;
//     case kKelonFanMedium: return stdAc::fanspeed_t::kMedium;
//     case kKelonFanMax:    return stdAc::fanspeed_t::kHigh;
//     default:              return stdAc::fanspeed_t::kAuto;
//   }
// }
