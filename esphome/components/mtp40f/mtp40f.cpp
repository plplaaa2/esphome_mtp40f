#include "mtp40f.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mtp40f {

static const char *const TAG = "mtp40f";

// 명령어 정의 (CRC는 동적으로 계산)
static const uint8_t MTP40F_COMMAND_GET_PPM[] = {0x42, 0x4D, 0xA0, 0x00, 0x03, 0x00, 0x00, 0x01, 0x32}; // 9

uint16_t MTP40FComponent::mtp40f_checksum_(const uint8_t *data, uint16_t length) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

void MTP40FComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MTP40F...");
  this->last_update_time_ = millis();
  this->last_error_ = MTP40F_OK;

  // 부팅시 자동 self calibration 설정
  if (self_calibration_) {
    this->enable_self_calibration();
  } else {
    this->disable_self_calibration();
  }
}

void MTP40FComponent::update() {
  uint32_t now_ms = millis();
  uint32_t warmup_ms = this->warmup_seconds_ * 1000;

  if (now_ms - this->last_update_time_ < warmup_ms) {
    ESP_LOGW(TAG, "MTP40F warming up, %u seconds left", (warmup_ms - (now_ms - this->last_update_time_)) / 1000);
    this->status_set_warning();
    return;
  }

  // 너무 빠른 읽기 방지(2초)
  if (now_ms - this->last_read_millis_ < 2000) {
    return;
  }
  this->last_read_millis_ = now_ms;

  this->last_error_ = MTP40F_OK;

  // CO2 값 읽기
  if (!this->mtp40f_request_(MTP40F_COMMAND_GET_PPM, sizeof(MTP40F_COMMAND_GET_PPM), this->response_buffer_, 14)) {
    ESP_LOGW(TAG, "Failed to read CO2 data from MTP40F! Last error: 0x%04X", this->last_error_);
    this->status_set_warning();
    return;
  }

  // CO2 파싱
  uint32_t ppm_value = (this->response_buffer_[7] << 24) | (this->response_buffer_[8] << 16) | (this->response_buffer_[9] << 8) | this->response_buffer_[10];
  uint8_t status_byte = this->response_buffer_[11];

  if (status_byte == 0x00) {
    ESP_LOGD(TAG, "MTP40F Received CO2=%u ppm", ppm_value);
    if (this->co2_sensor_ != nullptr) {
      this->co2_sensor_->publish_state(ppm_value);
    }
    this->status_clear_warning();
  } else {
    this->last_error_ = MTP40F_INVALID_GAS_LEVEL;
    ESP_LOGW(TAG, "MTP40F returned invalid gas level status: 0x%02X", status_byte);
    this->status_set_warning();
    return;
  }

  // 대기압 참조값 읽기 (동적 CRC)
  if (this->air_pressure_reference_sensor_ != nullptr) {
    this->last_error_ = MTP40F_OK;
    uint8_t cmd[9] = {0x42, 0x4D, 0xA0, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
    uint16_t checksum = 0;
    for (int i = 0; i < 7; i++) checksum += cmd[i];
    cmd[7] = checksum >> 8;
    cmd[8] = checksum & 0xFF;
    ESP_LOGD(TAG, "Sending Air Pressure Reference command: %02X %02X ... %02X %02X", cmd[0], cmd[1], cmd[7], cmd[8]);
    if (!this->mtp40f_request_(cmd, 9, this->response_buffer_, 11)) {
      ESP_LOGW(TAG, "Failed to read Air Pressure Reference from MTP40F! Last error: 0x%04X", this->last_error_);
    } else {
      uint16_t air_pressure_ref = (this->response_buffer_[7] << 8) | this->response_buffer_[8];
      ESP_LOGD(TAG, "MTP40F Received Air Pressure Reference=%u hPa", air_pressure_ref);
      this->air_pressure_reference_sensor_->publish_state(air_pressure_ref);
    }
  }
}  // ← 반드시 update 함수 끝에 이 중괄호!

// 외부 기압값
void MTP40FComponent::set_external_air_pressure_sensor(sensor::Sensor *sensor) {
  this->external_air_pressure_sensor_ = sensor;
  if (sensor != nullptr) {
    sensor->add_on_state_callback([this](float state) {
      this->on_external_air_pressure_update*

