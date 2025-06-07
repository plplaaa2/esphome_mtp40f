#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace mtp40f {

#define MTP40F_OK                   0x00
#define MTP40F_INVALID_AIR_PRESSURE 0x01
#define MTP40F_INVALID_GAS_LEVEL    0x02
#define MTP40F_INVALID_CRC          0x10
#define MTP40F_NO_STREAM            0x20
#define MTP40F_REQUEST_FAILED       0xFFFF

class MTP40FComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // 센서 설정
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_air_pressure_reference_sensor(sensor::Sensor *air_pressure_reference_sensor) { air_pressure_reference_sensor_ = air_pressure_reference_sensor; }

  // Self calibration 및 400ppm 보정
  void enable_self_calibration();
  void disable_self_calibration();
  void calibrate_400ppm();

  // 파라미터
  void set_self_calibration_enabled(bool enabled) { self_calibration_ = enabled; }
  void set_warmup_seconds(uint32_t seconds) { warmup_seconds_ = seconds; }

  // 디버깅
  int get_last_error() { return last_error_; }

 protected:
  uint16_t mtp40f_checksum_(const uint8_t *data, uint16_t length);
  bool mtp40f_request_(const uint8_t *full_command, size_t full_command_length, uint8_t *response_buffer, size_t response_length);

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *air_pressure_reference_sensor_{nullptr};
  bool self_calibration_{true};
  uint32_t warmup_seconds_{60};
  uint32_t last_update_time_{0};
  uint32_t last_read_millis_{0};
  int last_error_{MTP40F_OK};
  uint8_t response_buffer_[20];
};

// Switch class for self calibration ON/OFF
class MTP40FSelfCalibrationSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(MTP40FComponent *parent) { parent_ = parent; }
  void write_state(bool state) override {
    if (state) {
      parent_->enable_self_calibration();
    } else {
      parent_->disable_self_calibration();
    }
    publish_state(state);
  }
 protected:
  MTP40FComponent *parent_;
};

// 액션 클래스 (자동화에서 사용할 때)
template<typename... Ts>
class MTP40FCalibrate400ppmAction : public Action<Ts...> {
 public:
  MTP40FCalibrate400ppmAction(MTP40FComponent *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->calibrate_400ppm(); }
 protected:
  MTP40FComponent *parent_;
};

}  // namespace mtp40f
}  // namespace esphome
