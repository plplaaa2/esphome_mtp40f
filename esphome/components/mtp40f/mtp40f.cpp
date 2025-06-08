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
      this->on_external_air_pressure_update(state);
    });
  }
}

// 외부 기압값 읽기
void MTP40FComponent::on_external_air_pressure_update(float pressure_hpa) {
  if (pressure_hpa > 700 && pressure_hpa < 1100) { // 현실적인 범위 체크
    this->set_air_pressure_reference(static_cast<uint16_t>(pressure_hpa));
  }
}

// 기압 값 설정
void MTP40FComponent::set_air_pressure_reference(uint16_t hpa) {
  if (hpa < 700 || hpa > 1100) {
    ESP_LOGW(TAG, "Pressure value %u hPa out of range (700-1100)", hpa);
    return;
  }
  ESP_LOGD(TAG, "Setting Air Pressure Reference to %u hPa", hpa);

  uint8_t cmd[11] = {
      0x42, 0x4D, 0xA0,
      0x00, 0x01,        // Command
      0x00, 0x02,        // Data length
      static_cast<uint8_t>(hpa >> 8), static_cast<uint8_t>(hpa & 0xFF), // Pressure (big endian)
      0x00, 0x00         // Checksum placeholder
  };
  // Checksum of first 9 bytes
  uint16_t crc = 0;
  for (int i = 0; i < 9; i++) crc += cmd[i];
  cmd[9] = crc >> 8;
  cmd[10] = crc & 0xFF;

  // 전송, 응답 길이는 데이터시트 참고(없으면 0)
  this->mtp40f_request_(cmd, 11, this->response_buffer_, 0);
}

// 400ppm 보정(제로베이스)
void MTP40FComponent::calibrate_400ppm() {
  ESP_LOGD(TAG, "Calibrating MTP40F sensor to 400ppm (zero calibration, single point).");
  this->last_error_ = MTP40F_OK;
  uint8_t cmd[13] = {0x42, 0x4D, 0xA0, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0x90, 0x00, 0x00}; // 400ppm=0x0190
  uint16_t crc = mtp40f_checksum_(cmd, 11);
  cmd[11] = crc >> 8;
  cmd[12] = crc & 0xFF;
  if (!this->mtp40f_request_(cmd, 13, this->response_buffer_, 10)) {
    ESP_LOGW(TAG, "Failed to calibrate 400ppm! Last error: 0x%04X", this->last_error_);
  }
}

void MTP40FComponent::enable_self_calibration() {
  ESP_LOGD(TAG, "Enabling self-calibration on MTP40F...");
  this->last_error_ = MTP40F_OK;
  uint8_t cmd[10] = {0x42, 0x4D, 0xA0, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00};
  uint16_t crc = mtp40f_checksum_(cmd, 8);
  cmd[8] = crc >> 8;
  cmd[9] = crc & 0xFF;
  if (!this->mtp40f_request_(cmd, 10, this->response_buffer_, 9)) {
    ESP_LOGW(TAG, "Failed to enable self-calibration! Last error: 0x%04X", this->last_error_);
  }
}

void MTP40FComponent::disable_self_calibration() {
  ESP_LOGD(TAG, "Disabling self-calibration on MTP40F...");
  this->last_error_ = MTP40F_OK;
  uint8_t cmd[10] = {0x42, 0x4D, 0xA0, 0x00, 0x06, 0x00, 0x01, 0xFF, 0x00, 0x00};
  uint16_t crc = mtp40f_checksum_(cmd, 8);
  cmd[8] = crc >> 8;
  cmd[9] = crc & 0xFF;
  if (!this->mtp40f_request_(cmd, 10, this->response_buffer_, 9)) {
    ESP_LOGW(TAG, "Failed to disable self-calibration! Last error: 0x%04X", this->last_error_);
  }
}

bool MTP40FComponent::mtp40f_request_(const uint8_t *full_command, size_t full_command_length, uint8_t *response_buffer, size_t response_length) {
  // UART RX 버퍼 플러시
  while (this->available()) {
    this->read();
  }

  // 명령 전송
  this->write_array(full_command, full_command_length);
  this->flush();

  // 응답 읽기
  uint32_t start_time = millis();
  size_t bytes_read = 0;
  while (bytes_read < response_length) {
    if (millis() - start_time > 1000) {
      this->last_error_ = MTP40F_REQUEST_FAILED;
      ESP_LOGW(TAG, "MTP40F Read timeout! Expected %u bytes, got %u bytes.", response_length, bytes_read);
      return false;
    }
    if (this->available()) {
      response_buffer[bytes_read++] = this->read();
    }
    yield();
  }

  // 응답 CRC 체크
  if (response_length >= 2) {
    uint16_t received_checksum = (response_buffer[response_length - 2] << 8) | response_buffer[response_length - 1];
    uint16_t calculated_response_checksum = mtp40f_checksum_(response_buffer, response_length - 2);
    if (received_checksum != calculated_response_checksum) {
      this->last_error_ = MTP40F_INVALID_CRC;
      ESP_LOGW(TAG, "MTP40F Response checksum mismatch! Received 0x%04X, calculated 0x%04X", received_checksum, calculated_response_checksum);
      return false;
    }
  }
  return true;
}

float MTP40FComponent::get_setup_priority() const {
  return setup_priority::DATA;
}

void MTP40FComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MTP40F:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Air Pressure Reference", this->air_pressure_reference_sensor_);
  ESP_LOGCONFIG(TAG, "  Self-calibration enabled: %s", YESNO(this->self_calibration_));
  ESP_LOGCONFIG(TAG, "  Warmup time: %u seconds", this->warmup_seconds_);
}

}  // namespace mtp40f
}  // namespace esphome
