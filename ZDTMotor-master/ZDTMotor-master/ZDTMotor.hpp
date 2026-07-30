#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: Complete ZDT XS second-generation Emm CAN controller module
constructor_args:
  - can_name: "can1"
  - motor_id: 1
  - bitrate: 500000
  - configure_can: false
  - pulses_per_unit: 1.0
  - default_speed_rpm: 1000
  - default_acceleration: 0
  - duty_max_speed_rpm: 1000
  - checksum_mode: 0
  - receive_all_motor_ids: false
template_args: []
required_hardware: can_name/can/can1/CAN1
depends: []
=== END MANIFEST === */
// clang-format on

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "app_framework.hpp"
#include "can.hpp"
#include "libxr_cb.hpp"
#include "libxr_def.hpp"

class ZDTMotor : public LibXR::Application {
public:
  static constexpr uint8_t BROADCAST_ID = 0x00;
  static constexpr uint8_t DEFAULT_ID = 0x01;
  static constexpr uint8_t FIXED_CHECKSUM = 0x6B;
  static constexpr uint8_t FRAME_TAIL = FIXED_CHECKSUM;
  static constexpr uint16_t MAX_EMM_SPEED_RPM = 3000;
  static constexpr uint16_t MAX_CURRENT_MA = 5000;
  static constexpr size_t MAX_BATCH_BYTES = 512;
  static constexpr size_t MAX_RESPONSE_BYTES = 64;
  static constexpr size_t RX_ASSEMBLY_SLOTS = 8;

  enum class ChecksumMode : uint8_t {
    FIXED_6B = 0,
    XOR = 1,
    CRC8 = 2,
  };

  enum class Direction : uint8_t {
    CW = 0x00,
    CCW = 0x01,
  };

  enum class MotionMode : uint8_t {
    RELATIVE_TO_LAST_TARGET = 0x00,
    RELATIVE = RELATIVE_TO_LAST_TARGET,
    ABSOLUTE = 0x01,
    RELATIVE_TO_CURRENT = 0x02,
  };

  enum class ControlMode : uint8_t {
    OPEN_LOOP = 0x00,
    CLOSED_LOOP_FOC = 0x01,
  };

  enum class MotorType : uint8_t {
    STEP_0_9_DEG = 25,
    STEP_1_8_DEG = 50,
  };

  enum class FirmwareType : uint8_t {
    X = 0x00,
    EMM = 0x01,
    EMM_TURBO = 0x02,
  };

  enum class Dmx512MotionMode : uint8_t {
    RELATIVE_POSITION = 0x00,
    ABSOLUTE_POSITION = 0x01,
    CONTINUOUS_POSITION = ABSOLUTE_POSITION,
  };

  enum class OriginMode : uint8_t {
    SINGLE_TURN_NEAREST_ZERO = 0x00,
    SINGLE_TURN_DIRECTION_ZERO = 0x01,
    LIMIT_COLLISION_ZERO = 0x02,
    LIMIT_SWITCH_ZERO = 0x03,
    ABSOLUTE_COORDINATE_ZERO = 0x04,
    POWER_LOSS_POSITION = 0x05,
  };

  enum class SystemParam : uint8_t {
    FIRMWARE_AND_HARDWARE_VERSION = 0x1F,
    PHASE_RESISTANCE_AND_INDUCTANCE = 0x20,
    BUS_VOLTAGE = 0x24,
    BUS_CURRENT = 0x26,
    PHASE_CURRENT = 0x27,
    ENCODER_RAW = 0x29,
    REALTIME_PULSE = 0x30,
    ENCODER_LINEARIZED = 0x31,
    INPUT_PULSE = 0x32,
    TARGET_POSITION = 0x33,
    SET_POSITION = 0x34,
    SPEED = 0x35,
    POSITION = 0x36,
    POSITION_ERROR = 0x37,
    BATTERY_VOLTAGE = 0x38,
    TEMPERATURE = 0x39,
    SYSTEM_FLAGS = 0x3A,
    ORIGIN_FLAGS = 0x3B,
    SYSTEM_AND_ORIGIN_FLAGS = 0x3C,
    PIN_STATE = 0x3D,
  };

  enum class ResponseStatus : uint8_t {
    NONE = 0x00,
    OK = 0x02,
    ALREADY_AT_ORIGIN = 0x12,
    COMPLETE = 0x9F,
    PARAMETER_ERROR = 0xE2,
    FORMAT_ERROR = 0xEE,
  };

  enum class ParameterLockLevel : uint8_t {
    UNLOCKED = 0x00,
    COMMUNICATION_LOCKED = 0x01,
    ALL_PARAMETERS_LOCKED = 0x02,
    ALL_PARAMETERS_AND_CALIBRATION_LOCKED = 0x03,
  };

  enum class PulsePortMode : uint8_t {
    OFF = 0x00,
    OPEN_LOOP = 0x01,
    CLOSED_LOOP_FOC = 0x02,
    ORIGIN_LIMIT_INPUT = 0x03,
    LEFT_RIGHT_LIMIT_INPUT = 0x04,
  };

  enum class CommunicationPortMode : uint8_t {
    OFF = 0x00,
    AUTO_ORIGIN_LIMIT_AND_ALARM = 0x01,
    UART = 0x02,
    CAN = 0x03,
    LEFT_RIGHT_LIMIT_INPUT = 0x04,
  };

  enum class EnablePinLevel : uint8_t {
    LOW = 0x00,
    HIGH = 0x01,
    HOLD = 0x02,
  };

  enum class SerialBaudRate : uint8_t {
    BPS_9600 = 0,
    BPS_19200 = 1,
    BPS_25000 = 2,
    BPS_38400 = 3,
    BPS_57600 = 4,
    BPS_115200 = 5,
    BPS_256000 = 6,
    BPS_512000 = 7,
    BPS_921600 = 8,
  };

  enum class CanBitrate : uint8_t {
    KBPS_10 = 0,
    KBPS_20 = 1,
    KBPS_50 = 2,
    KBPS_83_333 = 3,
    KBPS_100 = 4,
    KBPS_125 = 5,
    KBPS_250 = 6,
    KBPS_500 = 7,
    KBPS_800 = 8,
    MBPS_1 = 9,
  };

  enum class CommunicationProtocol : uint8_t {
    FIXED_6B = 0,
    XOR = 1,
    CRC8 = 2,
    MODBUS_RTU = 3,
    FIXED_6B_AND_DMX512 = 4,
  };

  enum class ControlResponseMode : uint8_t {
    NONE = 0,
    RECEIVE = 1,
    REACHED = 2,
    BOTH = 3,
    OTHER = 4,
  };

  enum class ClogProtectionMode : uint8_t {
    DISABLED = 0,
    RELEASE_MOTOR = 1,
    RESET_POSITION_WITHOUT_RELEASE = 2,
  };

  struct BusinessFeedback {
    int32_t delta = 0;
    int32_t total = 0;
    float target_delta = 0.0F;
    float duty = 0.0F;
    bool closed_loop = true;
  };

  struct SignedMagnitude32 {
    bool negative = false;
    uint32_t magnitude = 0;

    [[nodiscard]] constexpr int64_t Value() const {
      return negative ? -static_cast<int64_t>(magnitude)
                      : static_cast<int64_t>(magnitude);
    }

    [[nodiscard]] constexpr double EmmDegrees() const {
      return static_cast<double>(Value()) * 360.0 / 65536.0;
    }
  };

  struct SignedSpeed {
    bool negative = false;
    uint16_t rpm = 0;

    [[nodiscard]] constexpr int32_t Value() const {
      return negative ? -static_cast<int32_t>(rpm)
                      : static_cast<int32_t>(rpm);
    }
  };

  struct FirmwareHardwareVersion {
    uint16_t firmware_version = 0;
    uint8_t hardware_series = 0;
    uint8_t hardware_type = 0;
    uint8_t hardware_version = 0;
    uint16_t hardware_raw = 0;
  };

  struct PhaseParameters {
    uint16_t resistance_milliohm = 0;
    uint16_t inductance_microhenry = 0;
  };

  struct MotorStatusFlags {
    uint8_t raw = 0;
    bool enabled = false;
    bool position_reached = false;
    bool clogged = false;
    bool clog_protection_triggered = false;
    bool left_limit_high = false;
    bool right_limit_high = false;
    bool power_loss_detected = false;
  };

  struct OriginStatusFlags {
    uint8_t raw = 0;
    bool encoder_ready = false;
    bool calibration_ready = false;
    bool returning_to_origin = false;
    bool return_to_origin_failed = false;
    bool over_temperature_protection = false;
    bool over_current_protection = false;
  };

  struct PinState {
    uint8_t raw = 0;
    bool enable_high = false;
    bool step_high = false;
    bool direction_high = false;
    bool direction_output = false;
  };

  struct OptionParamState {
    uint16_t raw = 0;
    MotorType motor_type = MotorType::STEP_1_8_DEG;
    FirmwareType firmware_type = FirmwareType::X;
    ControlMode control_mode = ControlMode::OPEN_LOOP;
    Direction positive_direction = Direction::CW;
    bool button_locked = false;
    bool input_scaled_by_ten = false;
    ParameterLockLevel parameter_lock = ParameterLockLevel::UNLOCKED;
  };

  struct OriginParams {
    bool save = false;
    OriginMode mode = OriginMode::SINGLE_TURN_NEAREST_ZERO;
    Direction direction = Direction::CW;
    uint16_t speed_rpm = 30;
    uint32_t timeout_ms = 10000;
    uint16_t collision_speed_rpm = 300;
    uint16_t collision_current_ma = 800;
    uint16_t collision_time_ms = 60;
    bool auto_return_on_power = false;
  };

  struct FastPositionParams {
    uint16_t speed_rpm = 1000;
    uint8_t acceleration = 0;
    MotionMode mode = MotionMode::RELATIVE_TO_LAST_TARGET;
    bool sync = false;
  };

  struct PowerOnVelocityParams {
    bool store = true;
    Direction direction = Direction::CW;
    uint16_t speed_rpm = 0;
    uint8_t acceleration = 0;
    bool enable_pin_control = false;
  };

  struct PIDParams {
    uint32_t kp = 0;
    uint32_t ki = 0;
    uint32_t kd = 0;
  };

  struct Dmx512Params {
    bool save = false;
    uint16_t start_channel = 192;
    uint8_t channel_count = 1;
    Dmx512MotionMode mode = Dmx512MotionMode::ABSOLUTE_POSITION;
    uint16_t speed_rpm = 1000;
    uint16_t acceleration_step = 1000;
    uint16_t velocity_step_rpm = 10;
    uint32_t position_step = 100;

    [[nodiscard]] constexpr uint16_t TotalChannels() const {
      return start_channel;
    }
  };

  struct ProtectionParams {
    bool save = false;
    uint16_t over_temperature_c = 100;
    uint16_t over_current_ma = 6600;
    uint16_t trigger_time_ms = 1000;
  };

  struct EmmSystemState {
    uint8_t total_bytes = 0;
    uint8_t parameter_count = 0;
    uint16_t bus_voltage_mv = 0;
    uint16_t phase_current_ma = 0;
    uint16_t encoder_linearized = 0;
    SignedMagnitude32 target_position{};
    SignedSpeed speed{};
    SignedMagnitude32 position{};
    SignedMagnitude32 position_error{};
    OriginStatusFlags origin_flags{};
    MotorStatusFlags motor_flags{};
  };

  struct EmmMotorConfig {
    uint8_t total_bytes = 0x21;
    uint8_t parameter_count = 0x15;
    MotorType motor_type = MotorType::STEP_1_8_DEG;
    PulsePortMode pulse_port_mode = PulsePortMode::CLOSED_LOOP_FOC;
    CommunicationPortMode communication_port_mode =
        CommunicationPortMode::UART;
    EnablePinLevel enable_pin_level = EnablePinLevel::HOLD;
    Direction positive_direction = Direction::CW;
    uint8_t microstep = 16;
    bool interpolation = true;
    uint8_t reserved = 0;
    uint16_t open_loop_current_ma = 1200;
    uint16_t closed_loop_current_ma = 3000;
    uint16_t closed_loop_max_voltage = 4000;
    SerialBaudRate serial_baud_rate = SerialBaudRate::BPS_115200;
    CanBitrate can_bitrate = CanBitrate::KBPS_500;
    uint8_t motor_id = DEFAULT_ID;
    CommunicationProtocol communication_protocol =
        CommunicationProtocol::FIXED_6B;
    ControlResponseMode response_mode = ControlResponseMode::RECEIVE;
    ClogProtectionMode clog_protection = ClogProtectionMode::RELEASE_MOTOR;
    uint16_t clog_speed_rpm = 8;
    uint16_t clog_current_ma = 2200;
    uint16_t clog_time_ms = 2000;
    uint16_t position_window_tenths_degree = 8;
  };

  struct Response {
    uint8_t motor_id = 0;
    uint8_t packet_index = 0;
    uint8_t command = 0;
    uint8_t length = 0;
    std::array<uint8_t, MAX_RESPONSE_BYTES> payload{};
    uint8_t checksum = 0;
    uint8_t frame_count = 0;
    ResponseStatus status = ResponseStatus::NONE;
    bool checksum_valid = false;
    bool valid = false;

    [[nodiscard]] constexpr bool IsError() const {
      return status == ResponseStatus::PARAMETER_ERROR ||
             status == ResponseStatus::FORMAT_ERROR;
    }

    [[nodiscard]] constexpr bool Succeeded() const {
      return valid && !IsError();
    }
  };

  using ResponseCallback = LibXR::Callback<const Response &>;

  static constexpr uint32_t MakeExtID(uint8_t motor_id,
                                      uint8_t packet_index) {
    return (static_cast<uint32_t>(motor_id) << 8U) |
           static_cast<uint32_t>(packet_index);
  }

  static constexpr uint8_t CalculateChecksum(ChecksumMode mode,
                                             uint8_t motor_id,
                                             uint8_t command,
                                             const uint8_t *payload,
                                             size_t payload_len) {
    if (mode == ChecksumMode::FIXED_6B) {
      return FIXED_CHECKSUM;
    }

    if (mode == ChecksumMode::XOR) {
      uint8_t checksum = static_cast<uint8_t>(motor_id ^ command);
      for (size_t index = 0; index < payload_len; ++index) {
        checksum ^= payload[index];
      }
      return checksum;
    }

    uint8_t checksum = motor_id;
    checksum = UpdateCRC8(checksum, command);
    for (size_t index = 0; index < payload_len; ++index) {
      checksum = UpdateCRC8(checksum, payload[index]);
    }
    return checksum;
  }

  template <size_t CAPACITY = MAX_BATCH_BYTES> class CommandBatch {
  public:
    explicit constexpr CommandBatch(
        ChecksumMode checksum_mode = ChecksumMode::FIXED_6B)
        : checksum_mode_(checksum_mode) {}

    LibXR::ErrorCode Clear() {
      size_ = 0;
      return LibXR::ErrorCode::OK;
    }

    void SetChecksumMode(ChecksumMode checksum_mode) {
      checksum_mode_ = checksum_mode;
    }

    [[nodiscard]] constexpr ChecksumMode GetChecksumMode() const {
      return checksum_mode_;
    }

    [[nodiscard]] size_t Size() const { return size_; }
    [[nodiscard]] constexpr size_t Capacity() const { return CAPACITY; }
    [[nodiscard]] const uint8_t *Data() const { return data_.data(); }

    LibXR::ErrorCode AppendRaw(uint8_t motor_id, uint8_t command,
                               const uint8_t *payload, size_t payload_len) {
      if (payload == nullptr && payload_len != 0) {
        return LibXR::ErrorCode::PTR_NULL;
      }
      if (size_ + payload_len + 3U > CAPACITY) {
        return LibXR::ErrorCode::NO_BUFF;
      }

      data_[size_++] = motor_id;
      data_[size_++] = command;
      for (size_t index = 0; index < payload_len; ++index) {
        data_[size_++] = payload[index];
      }
      data_[size_++] = ZDTMotor::CalculateChecksum(
          checksum_mode_, motor_id, command, payload, payload_len);
      return LibXR::ErrorCode::OK;
    }

    LibXR::ErrorCode AppendEncoderCalibration(uint8_t motor_id) {
      const uint8_t payload[] = {0x45};
      return AppendRaw(motor_id, 0x06, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendResetMotor(uint8_t motor_id) {
      const uint8_t payload[] = {0x97};
      return AppendRaw(motor_id, 0x08, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendResetCurrentPositionToZero(uint8_t motor_id) {
      const uint8_t payload[] = {0x6D};
      return AppendRaw(motor_id, 0x0A, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendResetProtection(uint8_t motor_id) {
      const uint8_t payload[] = {0x52};
      return AppendRaw(motor_id, 0x0E, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendRestoreFactorySettings(uint8_t motor_id) {
      const uint8_t payload[] = {0x5F};
      return AppendRaw(motor_id, 0x0F, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendEnable(uint8_t motor_id, bool enable,
                                  bool sync = false) {
      const uint8_t payload[] = {0xAB, static_cast<uint8_t>(enable),
                                 static_cast<uint8_t>(sync)};
      return AppendRaw(motor_id, 0xF3, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendVelocity(uint8_t motor_id, Direction direction,
                                    uint16_t speed_rpm, uint8_t acceleration,
                                    bool sync = false) {
      if (!ZDTMotor::IsValidSpeed(speed_rpm)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      const uint8_t payload[] = {
          static_cast<uint8_t>(direction),
          static_cast<uint8_t>(speed_rpm >> 8U),
          static_cast<uint8_t>(speed_rpm), acceleration,
          static_cast<uint8_t>(sync),
      };
      return AppendRaw(motor_id, 0xF6, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendPosition(
        uint8_t motor_id, Direction direction, uint16_t speed_rpm,
        uint8_t acceleration, uint32_t pulse_count,
        MotionMode mode = MotionMode::RELATIVE_TO_LAST_TARGET,
        bool sync = false) {
      if (!ZDTMotor::IsValidSpeed(speed_rpm) ||
          !ZDTMotor::IsValidMotionMode(mode)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      const uint8_t payload[] = {
          static_cast<uint8_t>(direction),
          static_cast<uint8_t>(speed_rpm >> 8U),
          static_cast<uint8_t>(speed_rpm), acceleration,
          static_cast<uint8_t>(pulse_count >> 24U),
          static_cast<uint8_t>(pulse_count >> 16U),
          static_cast<uint8_t>(pulse_count >> 8U),
          static_cast<uint8_t>(pulse_count), static_cast<uint8_t>(mode),
          static_cast<uint8_t>(sync),
      };
      return AppendRaw(motor_id, 0xFD, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendFastPositionConfig(
        uint8_t motor_id, const FastPositionParams &params) {
      if (!ZDTMotor::IsValidSpeed(params.speed_rpm) ||
          !ZDTMotor::IsValidMotionMode(params.mode)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      const uint8_t payload[] = {
          static_cast<uint8_t>(params.speed_rpm >> 8U),
          static_cast<uint8_t>(params.speed_rpm), params.acceleration,
          static_cast<uint8_t>(params.mode), static_cast<uint8_t>(params.sync),
      };
      return AppendRaw(motor_id, 0xF1, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendFastPosition(uint8_t motor_id,
                                        int32_t signed_pulse_count) {
      const uint32_t raw = static_cast<uint32_t>(signed_pulse_count);
      const uint8_t payload[] = {
          static_cast<uint8_t>(raw >> 24U),
          static_cast<uint8_t>(raw >> 16U),
          static_cast<uint8_t>(raw >> 8U), static_cast<uint8_t>(raw),
      };
      return AppendRaw(motor_id, 0xFC, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendStop(uint8_t motor_id, bool sync = false) {
      const uint8_t payload[] = {0x98, static_cast<uint8_t>(sync)};
      return AppendRaw(motor_id, 0xFE, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendSyncMotion(uint8_t motor_id = BROADCAST_ID) {
      const uint8_t payload[] = {0x66};
      return AppendRaw(motor_id, 0xFF, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendSetOriginZero(uint8_t motor_id,
                                         bool save = false) {
      const uint8_t payload[] = {0x88, static_cast<uint8_t>(save)};
      return AppendRaw(motor_id, 0x93, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendOriginReturn(
        uint8_t motor_id,
        OriginMode mode = OriginMode::SINGLE_TURN_NEAREST_ZERO,
        bool sync = false) {
      if (!ZDTMotor::IsValidOriginMode(mode)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      const uint8_t payload[] = {static_cast<uint8_t>(mode),
                                 static_cast<uint8_t>(sync)};
      return AppendRaw(motor_id, 0x9A, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendInterruptOriginReturn(uint8_t motor_id) {
      const uint8_t payload[] = {0x48};
      return AppendRaw(motor_id, 0x9C, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendReadOriginParams(uint8_t motor_id) {
      return AppendRaw(motor_id, 0x22, nullptr, 0);
    }

    LibXR::ErrorCode AppendModifyOriginParams(uint8_t motor_id,
                                              const OriginParams &params) {
      if (!ZDTMotor::IsValidOriginParams(params)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      std::array<uint8_t, 17> payload{};
      payload[0] = 0xAE;
      payload[1] = static_cast<uint8_t>(params.save);
      payload[2] = static_cast<uint8_t>(params.mode);
      payload[3] = static_cast<uint8_t>(params.direction);
      ZDTMotor::StoreBE16(payload.data() + 4, params.speed_rpm);
      ZDTMotor::StoreBE32(payload.data() + 6, params.timeout_ms);
      ZDTMotor::StoreBE16(payload.data() + 10,
                          params.collision_speed_rpm);
      ZDTMotor::StoreBE16(payload.data() + 12,
                          params.collision_current_ma);
      ZDTMotor::StoreBE16(payload.data() + 14,
                          params.collision_time_ms);
      payload[16] = static_cast<uint8_t>(params.auto_return_on_power);
      return AppendRaw(motor_id, 0x4C, payload.data(), payload.size());
    }

    LibXR::ErrorCode AppendReadSystemParam(uint8_t motor_id,
                                           SystemParam param) {
      return AppendRaw(motor_id, static_cast<uint8_t>(param), nullptr, 0);
    }

    LibXR::ErrorCode AppendAutoReturnSystemParam(uint8_t motor_id,
                                                 SystemParam param,
                                                 uint16_t interval_ms) {
      const uint8_t payload[] = {
          0x18, static_cast<uint8_t>(param),
          static_cast<uint8_t>(interval_ms >> 8U),
          static_cast<uint8_t>(interval_ms),
      };
      return AppendRaw(motor_id, 0x11, payload, sizeof(payload));
    }

    LibXR::ErrorCode AppendPowerOnVelocity(
        uint8_t motor_id, const PowerOnVelocityParams &params) {
      if (!ZDTMotor::IsValidSpeed(params.speed_rpm)) {
        return LibXR::ErrorCode::OUT_OF_RANGE;
      }
      const uint8_t payload[] = {
          0x1C,
          static_cast<uint8_t>(params.store),
          static_cast<uint8_t>(params.direction),
          static_cast<uint8_t>(params.speed_rpm >> 8U),
          static_cast<uint8_t>(params.speed_rpm),
          params.acceleration,
          static_cast<uint8_t>(params.enable_pin_control),
      };
      return AppendRaw(motor_id, 0xF7, payload, sizeof(payload));
    }

  private:
    std::array<uint8_t, CAPACITY> data_{};
    size_t size_ = 0;
    ChecksumMode checksum_mode_ = ChecksumMode::FIXED_6B;
  };

  ZDTMotor(LibXR::HardwareContainer &hardware,
           LibXR::ApplicationManager &application_manager,
           const char *can_name = "can1", uint8_t motor_id = DEFAULT_ID,
           uint32_t bitrate = 500000, bool configure_can = false,
           float pulses_per_unit = 1.0F,
           uint16_t default_speed_rpm = 1000,
           uint8_t default_acceleration = 0,
           uint16_t duty_max_speed_rpm = 1000,
           uint8_t checksum_mode = 0, bool receive_all_motor_ids = false)
      : can_(hardware.template FindOrExit<LibXR::CAN>(
            {can_name, "can", "can1", "CAN1"})),
        motor_id_(motor_id),
        checksum_mode_(DecodeChecksumMode(checksum_mode)),
        receive_all_motor_ids_(receive_all_motor_ids),
        pulses_per_unit_(pulses_per_unit),
        default_speed_rpm_(default_speed_rpm),
        default_acceleration_(default_acceleration),
        duty_max_speed_rpm_(duty_max_speed_rpm),
        rx_callback_(LibXR::CAN::Callback::Create(OnCanMessage, this)) {
    if (pulses_per_unit_ <= 0.0F) {
      pulses_per_unit_ = 1.0F;
    }
    if (!IsValidSpeed(default_speed_rpm_)) {
      default_speed_rpm_ = MAX_EMM_SPEED_RPM;
    }
    if (!IsValidSpeed(duty_max_speed_rpm_)) {
      duty_max_speed_rpm_ = MAX_EMM_SPEED_RPM;
    }

    if (configure_can) {
      can_->SetConfig({.bitrate = bitrate});
    } else {
      UNUSED(bitrate);
    }

    can_->Register(rx_callback_, LibXR::CAN::Type::EXTENDED);
    application_manager.Register(*this);
  }

  void OnMonitor() override {
    LibXR::CAN::ErrorState state{};
    if (can_->GetErrorState(state) == LibXR::ErrorCode::OK) {
      can_error_state_ = state;
    }
  }

  LibXR::ErrorCode Enable() { return Enable(true); }

  void Disable() {
    feedback_.target_delta = 0.0F;
    feedback_.duty = 0.0F;
    feedback_.closed_loop = true;
    open_loop_update_pending_ = false;
    (void)Enable(false, false);
  }

  void Relax() {
    feedback_.target_delta = 0.0F;
    feedback_.duty = 0.0F;
    feedback_.closed_loop = false;
    open_loop_update_pending_ = false;
    (void)Enable(false, false);
  }

  void SetTargetDelta(float target_delta) {
    feedback_.target_delta = target_delta;
    feedback_.duty = 0.0F;
    feedback_.closed_loop = true;
    open_loop_update_pending_ = false;
  }

  void SetOpenLoopDuty(float duty) {
    feedback_.target_delta = 0.0F;
    feedback_.duty = ClampDuty(duty);
    feedback_.closed_loop = false;
    open_loop_update_pending_ = true;
  }

  LibXR::ErrorCode Update(float dt_seconds) {
    UNUSED(dt_seconds);
    if (feedback_.duty != 0.0F || open_loop_update_pending_) {
      const uint16_t speed_rpm =
          DutyToSpeedRPM(feedback_.duty, duty_max_speed_rpm_);
      open_loop_update_pending_ = false;
      if (speed_rpm == 0) {
        return Stop(false);
      }
      return Velocity(DirectionFromSigned(feedback_.duty), speed_rpm,
                      default_acceleration_, false);
    }

    if (feedback_.target_delta == 0.0F) {
      return LibXR::ErrorCode::OK;
    }

    const uint32_t pulse_count =
        TargetDeltaToPulseCount(feedback_.target_delta, pulses_per_unit_);
    if (pulse_count == 0) {
      feedback_.target_delta = 0.0F;
      return LibXR::ErrorCode::OK;
    }

    const auto result = Position(
        DirectionFromSigned(feedback_.target_delta), default_speed_rpm_,
        default_acceleration_, pulse_count,
        MotionMode::RELATIVE_TO_LAST_TARGET, false);
    if (result == LibXR::ErrorCode::OK) {
      feedback_.delta = SignedPulseDelta(feedback_.target_delta, pulse_count);
      feedback_.total = SaturatingAdd(feedback_.total, feedback_.delta);
      feedback_.target_delta = 0.0F;
    }
    return result;
  }

  [[nodiscard]] const BusinessFeedback &GetFeedback() const {
    return feedback_;
  }

  [[nodiscard]] float GetTargetDelta() const { return feedback_.target_delta; }

  [[nodiscard]] bool HasActiveCommand() const {
    return feedback_.target_delta != 0.0F || feedback_.duty != 0.0F ||
           open_loop_update_pending_;
  }

  void SetBusinessControlConfig(float pulses_per_unit,
                                uint16_t default_speed_rpm,
                                uint8_t default_acceleration,
                                uint16_t duty_max_speed_rpm) {
    pulses_per_unit_ = (pulses_per_unit > 0.0F) ? pulses_per_unit : 1.0F;
    default_speed_rpm_ =
        IsValidSpeed(default_speed_rpm) ? default_speed_rpm : MAX_EMM_SPEED_RPM;
    default_acceleration_ = default_acceleration;
    duty_max_speed_rpm_ = IsValidSpeed(duty_max_speed_rpm)
                              ? duty_max_speed_rpm
                              : MAX_EMM_SPEED_RPM;
  }

  [[nodiscard]] uint8_t MotorID() const { return motor_id_; }
  void SetMotorID(uint8_t motor_id) { motor_id_ = motor_id; }

  void SetChecksumMode(ChecksumMode checksum_mode) {
    checksum_mode_ = checksum_mode;
    ResetAssemblies();
  }

  [[nodiscard]] ChecksumMode GetChecksumMode() const {
    return checksum_mode_;
  }

  void SetReceiveAllMotorIDs(bool receive_all_motor_ids) {
    receive_all_motor_ids_ = receive_all_motor_ids;
  }

  [[nodiscard]] bool ReceivesAllMotorIDs() const {
    return receive_all_motor_ids_ || motor_id_ == BROADCAST_ID;
  }

  void SetResponseCallback(ResponseCallback callback) {
    response_callback_ = callback;
  }

  [[nodiscard]] Response LastResponse() const { return last_response_; }
  [[nodiscard]] uint32_t RxCount() const { return rx_count_; }
  [[nodiscard]] uint32_t ResponseCount() const { return response_count_; }
  [[nodiscard]] uint32_t ChecksumErrorCount() const {
    return checksum_error_count_;
  }
  [[nodiscard]] uint32_t SequenceErrorCount() const {
    return sequence_error_count_;
  }
  [[nodiscard]] uint32_t OverflowCount() const { return overflow_count_; }
  [[nodiscard]] LibXR::CAN::ErrorState LastCanErrorState() const {
    return can_error_state_;
  }

  LibXR::ErrorCode TriggerEncoderCalibration() {
    const uint8_t payload[] = {0x45};
    return SendCommand(0x06, payload, sizeof(payload));
  }

  LibXR::ErrorCode ResetMotor() {
    const uint8_t payload[] = {0x97};
    return SendCommand(0x08, payload, sizeof(payload));
  }

  LibXR::ErrorCode ResetCurrentPositionToZero() {
    const uint8_t payload[] = {0x6D};
    return SendCommand(0x0A, payload, sizeof(payload));
  }

  LibXR::ErrorCode ResetProtection() {
    const uint8_t payload[] = {0x52};
    return SendCommand(0x0E, payload, sizeof(payload));
  }

  LibXR::ErrorCode ResetClogProtection() { return ResetProtection(); }

  LibXR::ErrorCode RestoreFactorySettings() {
    const uint8_t payload[] = {0x5F};
    return SendCommand(0x0F, payload, sizeof(payload));
  }

  LibXR::ErrorCode Enable(bool enable, bool sync = false) {
    const uint8_t payload[] = {0xAB, static_cast<uint8_t>(enable),
                               static_cast<uint8_t>(sync)};
    return SendCommand(0xF3, payload, sizeof(payload));
  }

  LibXR::ErrorCode Velocity(Direction direction, uint16_t speed_rpm,
                            uint8_t acceleration, bool sync = false) {
    if (!IsValidSpeed(speed_rpm)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(direction),
        static_cast<uint8_t>(speed_rpm >> 8U),
        static_cast<uint8_t>(speed_rpm), acceleration,
        static_cast<uint8_t>(sync),
    };
    return SendCommand(0xF6, payload, sizeof(payload));
  }

  LibXR::ErrorCode Position(
      Direction direction, uint16_t speed_rpm, uint8_t acceleration,
      uint32_t pulse_count,
      MotionMode mode = MotionMode::RELATIVE_TO_LAST_TARGET,
      bool sync = false) {
    if (!IsValidSpeed(speed_rpm) || !IsValidMotionMode(mode)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(direction),
        static_cast<uint8_t>(speed_rpm >> 8U),
        static_cast<uint8_t>(speed_rpm), acceleration,
        static_cast<uint8_t>(pulse_count >> 24U),
        static_cast<uint8_t>(pulse_count >> 16U),
        static_cast<uint8_t>(pulse_count >> 8U),
        static_cast<uint8_t>(pulse_count), static_cast<uint8_t>(mode),
        static_cast<uint8_t>(sync),
    };
    return SendCommand(0xFD, payload, sizeof(payload));
  }

  LibXR::ErrorCode ConfigureFastPosition(const FastPositionParams &params) {
    if (!IsValidSpeed(params.speed_rpm) ||
        !IsValidMotionMode(params.mode)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(params.speed_rpm >> 8U),
        static_cast<uint8_t>(params.speed_rpm), params.acceleration,
        static_cast<uint8_t>(params.mode), static_cast<uint8_t>(params.sync),
    };
    return SendCommand(0xF1, payload, sizeof(payload));
  }

  LibXR::ErrorCode FastPosition(int32_t signed_pulse_count) {
    const uint32_t raw = static_cast<uint32_t>(signed_pulse_count);
    const uint8_t payload[] = {
        static_cast<uint8_t>(raw >> 24U),
        static_cast<uint8_t>(raw >> 16U),
        static_cast<uint8_t>(raw >> 8U), static_cast<uint8_t>(raw),
    };
    return SendCommand(0xFC, payload, sizeof(payload));
  }

  LibXR::ErrorCode Stop(bool sync = false) {
    const uint8_t payload[] = {0x98, static_cast<uint8_t>(sync)};
    return SendCommand(0xFE, payload, sizeof(payload));
  }

  LibXR::ErrorCode SynchronousMotion(
      uint8_t target_motor_id = BROADCAST_ID) {
    const uint8_t payload[] = {0x66};
    return SendRaw(target_motor_id, 0xFF, payload, sizeof(payload));
  }

  LibXR::ErrorCode SetOriginZero(bool save = false) {
    const uint8_t payload[] = {0x88, static_cast<uint8_t>(save)};
    return SendCommand(0x93, payload, sizeof(payload));
  }

  LibXR::ErrorCode TriggerOriginReturn(
      OriginMode mode = OriginMode::SINGLE_TURN_NEAREST_ZERO,
      bool sync = false) {
    if (!IsValidOriginMode(mode)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {static_cast<uint8_t>(mode),
                               static_cast<uint8_t>(sync)};
    return SendCommand(0x9A, payload, sizeof(payload));
  }

  LibXR::ErrorCode InterruptOriginReturn() {
    const uint8_t payload[] = {0x48};
    return SendCommand(0x9C, payload, sizeof(payload));
  }

  LibXR::ErrorCode ReadOriginParams() {
    return SendCommand(0x22, nullptr, 0);
  }

  LibXR::ErrorCode ModifyOriginParams(const OriginParams &params) {
    if (!IsValidOriginParams(params)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    std::array<uint8_t, 17> payload{};
    payload[0] = 0xAE;
    payload[1] = static_cast<uint8_t>(params.save);
    payload[2] = static_cast<uint8_t>(params.mode);
    payload[3] = static_cast<uint8_t>(params.direction);
    StoreBE16(payload.data() + 4, params.speed_rpm);
    StoreBE32(payload.data() + 6, params.timeout_ms);
    StoreBE16(payload.data() + 10, params.collision_speed_rpm);
    StoreBE16(payload.data() + 12, params.collision_current_ma);
    StoreBE16(payload.data() + 14, params.collision_time_ms);
    payload[16] = static_cast<uint8_t>(params.auto_return_on_power);
    return SendCommand(0x4C, payload.data(), payload.size());
  }

  LibXR::ErrorCode ReadSystemParam(SystemParam param) {
    return SendCommand(static_cast<uint8_t>(param), nullptr, 0);
  }

  LibXR::ErrorCode AutoReturnSystemParam(SystemParam param,
                                         uint16_t interval_ms) {
    const uint8_t payload[] = {
        0x18, static_cast<uint8_t>(param),
        static_cast<uint8_t>(interval_ms >> 8U),
        static_cast<uint8_t>(interval_ms),
    };
    return SendCommand(0x11, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyMotorID(bool save, uint8_t new_motor_id) {
    if (new_motor_id == BROADCAST_ID) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {0x4B, static_cast<uint8_t>(save),
                               new_motor_id};
    return SendCommand(0xAE, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyMicroStep(bool save, uint8_t microstep) {
    const uint8_t payload[] = {0x8A, static_cast<uint8_t>(save), microstep};
    return SendCommand(0x84, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyPowerDownFlag(bool power_down) {
    const uint8_t payload[] = {static_cast<uint8_t>(power_down)};
    return SendCommand(0x50, payload, sizeof(payload));
  }

  LibXR::ErrorCode ReadOptionParamState() {
    return SendCommand(0x1A, nullptr, 0);
  }

  LibXR::ErrorCode ModifyMotorType(bool save, MotorType type) {
    const uint8_t payload[] = {0x35, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(type)};
    return SendCommand(0xD7, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyFirmwareType(bool save, FirmwareType type) {
    const uint8_t payload[] = {0x69, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(type)};
    return SendCommand(0xD5, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyControlMode(bool save, ControlMode mode) {
    const uint8_t payload[] = {0xA6, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(mode)};
    return SendCommand(0x46, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyControlModeLegacy(bool save, ControlMode mode) {
    const uint8_t payload[] = {0x69, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(mode)};
    return SendCommand(0x46, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyMotorDirection(bool save, Direction direction) {
    const uint8_t payload[] = {0x60, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(direction)};
    return SendCommand(0xD4, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyLockButton(bool save, bool lock) {
    const uint8_t payload[] = {0xB3, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(lock)};
    return SendCommand(0xD0, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyVelocityScale(bool save, bool divide_by_ten) {
    const uint8_t payload[] = {0x71, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(divide_by_ten)};
    return SendCommand(0x4F, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyStartupVelocityCheck(bool save, bool enable) {
    return ModifyVelocityScale(save, enable);
  }

  LibXR::ErrorCode ModifyOpenLoopCurrent(bool save, uint16_t current_ma) {
    if (current_ma > MAX_CURRENT_MA) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        0x33, static_cast<uint8_t>(save),
        static_cast<uint8_t>(current_ma >> 8U),
        static_cast<uint8_t>(current_ma),
    };
    return SendCommand(0x44, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyFOCCurrent(bool save, uint16_t current_ma) {
    if (current_ma > MAX_CURRENT_MA) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        0x66, static_cast<uint8_t>(save),
        static_cast<uint8_t>(current_ma >> 8U),
        static_cast<uint8_t>(current_ma),
    };
    return SendCommand(0x45, payload, sizeof(payload));
  }

  LibXR::ErrorCode ReadPIDParams() {
    return SendCommand(0x21, nullptr, 0);
  }

  LibXR::ErrorCode ModifyPIDParams(bool save, uint32_t kp, uint32_t ki,
                                   uint32_t kd) {
    std::array<uint8_t, 14> payload{};
    payload[0] = 0xC3;
    payload[1] = static_cast<uint8_t>(save);
    StoreBE32(payload.data() + 2, kp);
    StoreBE32(payload.data() + 6, ki);
    StoreBE32(payload.data() + 10, kd);
    return SendCommand(0x4A, payload.data(), payload.size());
  }

  LibXR::ErrorCode ModifyPIDParams(bool save, const PIDParams &params) {
    return ModifyPIDParams(save, params.kp, params.ki, params.kd);
  }

  LibXR::ErrorCode ReadDMX512Params() {
    const uint8_t payload[] = {0x78};
    return SendCommand(0x49, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyDMX512Params(const Dmx512Params &params) {
    if (params.channel_count < 1 || params.channel_count > 2 ||
        !IsValidSpeed(params.speed_rpm)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    std::array<uint8_t, 16> payload{};
    payload[0] = 0x90;
    payload[1] = static_cast<uint8_t>(params.save);
    StoreBE16(payload.data() + 2, params.start_channel);
    payload[4] = params.channel_count;
    payload[5] = static_cast<uint8_t>(params.mode);
    StoreBE16(payload.data() + 6, params.speed_rpm);
    StoreBE16(payload.data() + 8, params.acceleration_step);
    StoreBE16(payload.data() + 10, params.velocity_step_rpm);
    StoreBE32(payload.data() + 12, params.position_step);
    return SendCommand(0xD9, payload.data(), payload.size());
  }

  LibXR::ErrorCode ReadPositionWindow() {
    return SendCommand(0x41, nullptr, 0);
  }

  LibXR::ErrorCode ModifyPositionWindow(bool save, uint16_t window) {
    const uint8_t payload[] = {
        0x07, static_cast<uint8_t>(save),
        static_cast<uint8_t>(window >> 8U), static_cast<uint8_t>(window),
    };
    return SendCommand(0xD1, payload, sizeof(payload));
  }

  LibXR::ErrorCode ReadProtectionParams() {
    return SendCommand(0x13, nullptr, 0);
  }

  LibXR::ErrorCode ModifyProtectionParams(const ProtectionParams &params) {
    std::array<uint8_t, 8> payload{};
    payload[0] = 0x56;
    payload[1] = static_cast<uint8_t>(params.save);
    StoreBE16(payload.data() + 2, params.over_temperature_c);
    StoreBE16(payload.data() + 4, params.over_current_ma);
    StoreBE16(payload.data() + 6, params.trigger_time_ms);
    return SendCommand(0xD3, payload.data(), payload.size());
  }

  LibXR::ErrorCode ReadHeartbeatProtection() {
    return SendCommand(0x16, nullptr, 0);
  }

  LibXR::ErrorCode ModifyHeartbeatProtection(bool save,
                                              uint32_t heartbeat_ms) {
    std::array<uint8_t, 6> payload{};
    payload[0] = 0x38;
    payload[1] = static_cast<uint8_t>(save);
    StoreBE32(payload.data() + 2, heartbeat_ms);
    return SendCommand(0x68, payload.data(), payload.size());
  }

  LibXR::ErrorCode ReadIntegralLimit() {
    return SendCommand(0x23, nullptr, 0);
  }

  LibXR::ErrorCode ModifyIntegralLimit(bool save, uint32_t limit) {
    std::array<uint8_t, 6> payload{};
    payload[0] = 0x57;
    payload[1] = static_cast<uint8_t>(save);
    StoreBE32(payload.data() + 2, limit);
    return SendCommand(0x4B, payload.data(), payload.size());
  }

  LibXR::ErrorCode ReadCollisionOriginReturnAngle() {
    return SendCommand(0x3F, nullptr, 0);
  }

  LibXR::ErrorCode ModifyCollisionOriginReturnAngle(bool save,
                                                     uint16_t tenths_degree) {
    const uint8_t payload[] = {
        0xAC, static_cast<uint8_t>(save),
        static_cast<uint8_t>(tenths_degree >> 8U),
        static_cast<uint8_t>(tenths_degree),
    };
    return SendCommand(0x5C, payload, sizeof(payload));
  }

  LibXR::ErrorCode BroadcastReadMotorID() {
    return SendRaw(BROADCAST_ID, 0x15, nullptr, 0);
  }

  LibXR::ErrorCode ModifyParameterLock(bool save,
                                       ParameterLockLevel level) {
    const uint8_t payload[] = {0x4B, static_cast<uint8_t>(save),
                               static_cast<uint8_t>(level)};
    return SendCommand(0xD6, payload, sizeof(payload));
  }

  LibXR::ErrorCode ConfigurePowerOnVelocity(
      const PowerOnVelocityParams &params) {
    if (!IsValidSpeed(params.speed_rpm)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }
    const uint8_t payload[] = {
        0x1C,
        static_cast<uint8_t>(params.store),
        static_cast<uint8_t>(params.direction),
        static_cast<uint8_t>(params.speed_rpm >> 8U),
        static_cast<uint8_t>(params.speed_rpm),
        params.acceleration,
        static_cast<uint8_t>(params.enable_pin_control),
    };
    return SendCommand(0xF7, payload, sizeof(payload));
  }

  LibXR::ErrorCode ClearPowerOnVelocity() {
    PowerOnVelocityParams params{};
    params.store = false;
    return ConfigurePowerOnVelocity(params);
  }

  LibXR::ErrorCode ReadSystemStateParams() {
    const uint8_t payload[] = {0x7A};
    return SendCommand(0x43, payload, sizeof(payload));
  }

  LibXR::ErrorCode ReadMotorConfigParams() {
    const uint8_t payload[] = {0x6C};
    return SendCommand(0x42, payload, sizeof(payload));
  }

  LibXR::ErrorCode ModifyMotorConfigParams(bool save,
                                            const EmmMotorConfig &config) {
    if (!IsValidMotorConfig(config)) {
      return LibXR::ErrorCode::OUT_OF_RANGE;
    }

    std::array<uint8_t, 30> payload{};
    payload[0] = 0xD1;
    payload[1] = static_cast<uint8_t>(save);
    payload[2] = static_cast<uint8_t>(config.motor_type);
    payload[3] = static_cast<uint8_t>(config.pulse_port_mode);
    payload[4] = static_cast<uint8_t>(config.communication_port_mode);
    payload[5] = static_cast<uint8_t>(config.enable_pin_level);
    payload[6] = static_cast<uint8_t>(config.positive_direction);
    payload[7] = config.microstep;
    payload[8] = static_cast<uint8_t>(config.interpolation);
    payload[9] = config.reserved;
    StoreBE16(payload.data() + 10, config.open_loop_current_ma);
    StoreBE16(payload.data() + 12, config.closed_loop_current_ma);
    StoreBE16(payload.data() + 14, config.closed_loop_max_voltage);
    payload[16] = static_cast<uint8_t>(config.serial_baud_rate);
    payload[17] = static_cast<uint8_t>(config.can_bitrate);
    payload[18] = 0x00;
    payload[19] = static_cast<uint8_t>(config.communication_protocol);
    payload[20] = static_cast<uint8_t>(config.response_mode);
    payload[21] = static_cast<uint8_t>(config.clog_protection);
    StoreBE16(payload.data() + 22, config.clog_speed_rpm);
    StoreBE16(payload.data() + 24, config.clog_current_ma);
    StoreBE16(payload.data() + 26, config.clog_time_ms);
    StoreBE16(payload.data() + 28,
              config.position_window_tenths_degree);
    return SendCommand(0x48, payload.data(), payload.size());
  }

  static LibXR::ErrorCode DecodeStatus(const Response &response,
                                       ResponseStatus &status) {
    if (!response.valid) {
      return (response.frame_count == 0) ? LibXR::ErrorCode::NO_RESPONSE
                                         : LibXR::ErrorCode::CHECK_ERR;
    }
    if (response.length != 1) {
      return LibXR::ErrorCode::SIZE_ERR;
    }
    status = ClassifyStatus(response.command, response.payload[0], 1);
    return (status == ResponseStatus::NONE) ? LibXR::ErrorCode::ARG_ERR
                                            : LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeUnsigned16(const Response &response,
                                           uint8_t expected_command,
                                           uint16_t &value) {
    const auto result = ValidateDecode(response, expected_command, 2);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    value = LoadBE16(response.payload.data());
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeUnsigned32(const Response &response,
                                           uint8_t expected_command,
                                           uint32_t &value) {
    const auto result = ValidateDecode(response, expected_command, 4);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    value = LoadBE32(response.payload.data());
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeSignedMagnitude32(
      const Response &response, uint8_t expected_command,
      SignedMagnitude32 &value) {
    const auto result = ValidateDecode(response, expected_command, 5);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    value.negative = response.payload[0] != 0;
    value.magnitude = LoadBE32(response.payload.data() + 1);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeSpeed(const Response &response,
                                      SignedSpeed &speed) {
    const auto result = ValidateDecode(response, 0x35, 3);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    speed.negative = response.payload[0] != 0;
    speed.rpm = LoadBE16(response.payload.data() + 1);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeTemperature(const Response &response,
                                            int16_t &temperature_c) {
    const auto result = ValidateDecode(response, 0x39, 2);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    const int16_t magnitude = response.payload[1];
    temperature_c = (response.payload[0] != 0) ? -magnitude : magnitude;
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeFirmwareHardwareVersion(
      const Response &response, FirmwareHardwareVersion &version) {
    const auto result = ValidateDecode(response, 0x1F, 4);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    version.firmware_version = LoadBE16(response.payload.data());
    version.hardware_raw = LoadBE16(response.payload.data() + 2);
    version.hardware_series =
        static_cast<uint8_t>((version.hardware_raw >> 12U) & 0x0FU);
    version.hardware_type =
        static_cast<uint8_t>((version.hardware_raw >> 8U) & 0x0FU);
    version.hardware_version = static_cast<uint8_t>(version.hardware_raw);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodePhaseParameters(
      const Response &response, PhaseParameters &params) {
    const auto result = ValidateDecode(response, 0x20, 4);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    params.resistance_milliohm = LoadBE16(response.payload.data());
    params.inductance_microhenry = LoadBE16(response.payload.data() + 2);
    return LibXR::ErrorCode::OK;
  }

  static constexpr MotorStatusFlags ParseMotorStatusFlags(uint8_t raw) {
    return {
        .raw = raw,
        .enabled = (raw & 0x01U) != 0,
        .position_reached = (raw & 0x02U) != 0,
        .clogged = (raw & 0x04U) != 0,
        .clog_protection_triggered = (raw & 0x08U) != 0,
        .left_limit_high = (raw & 0x10U) != 0,
        .right_limit_high = (raw & 0x20U) != 0,
        .power_loss_detected = (raw & 0x80U) != 0,
    };
  }

  static constexpr OriginStatusFlags ParseOriginStatusFlags(uint8_t raw) {
    return {
        .raw = raw,
        .encoder_ready = (raw & 0x01U) != 0,
        .calibration_ready = (raw & 0x02U) != 0,
        .returning_to_origin = (raw & 0x04U) != 0,
        .return_to_origin_failed = (raw & 0x08U) != 0,
        .over_temperature_protection = (raw & 0x10U) != 0,
        .over_current_protection = (raw & 0x20U) != 0,
    };
  }

  static constexpr PinState ParsePinState(uint8_t raw) {
    return {
        .raw = raw,
        .enable_high = (raw & 0x01U) != 0,
        .step_high = (raw & 0x04U) != 0,
        .direction_high = (raw & 0x10U) != 0,
        .direction_output = (raw & 0x20U) != 0,
    };
  }

  static LibXR::ErrorCode DecodeMotorStatusFlags(
      const Response &response, MotorStatusFlags &flags) {
    const auto result = ValidateDecode(response, 0x3A, 1);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    flags = ParseMotorStatusFlags(response.payload[0]);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeOriginStatusFlags(
      const Response &response, OriginStatusFlags &flags) {
    const auto result = ValidateDecode(response, 0x3B, 1);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    flags = ParseOriginStatusFlags(response.payload[0]);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeCombinedStatusFlags(
      const Response &response, OriginStatusFlags &origin_flags,
      MotorStatusFlags &motor_flags) {
    const auto result = ValidateDecode(response, 0x3C, 2);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    origin_flags = ParseOriginStatusFlags(response.payload[0]);
    motor_flags = ParseMotorStatusFlags(response.payload[1]);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodePinState(const Response &response,
                                         PinState &state) {
    const auto result = ValidateDecode(response, 0x3D, 1);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    state = ParsePinState(response.payload[0]);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeOptionParamState(
      const Response &response, OptionParamState &state) {
    const auto result = ValidateDecodeRange(response, 0x1A, 1, 2);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    state.raw = response.payload[0];
    if (response.length >= 2) {
      state.raw |= static_cast<uint16_t>(response.payload[1]) << 8U;
    }
    state.motor_type = (state.raw & 0x0001U) != 0
                           ? MotorType::STEP_0_9_DEG
                           : MotorType::STEP_1_8_DEG;
    state.firmware_type = (state.raw & 0x0002U) != 0
                              ? FirmwareType::EMM
                              : FirmwareType::X;
    state.control_mode = (state.raw & 0x0004U) != 0
                             ? ControlMode::CLOSED_LOOP_FOC
                             : ControlMode::OPEN_LOOP;
    state.positive_direction = (state.raw & 0x0010U) != 0
                                   ? Direction::CCW
                                   : Direction::CW;
    state.button_locked = (state.raw & 0x0020U) != 0;
    state.input_scaled_by_ten = (state.raw & 0x0080U) != 0;
    state.parameter_lock = static_cast<ParameterLockLevel>(
        static_cast<uint8_t>((state.raw >> 8U) & 0x03U));
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeOriginParams(const Response &response,
                                             OriginParams &params) {
    const auto result = ValidateDecode(response, 0x22, 15);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    params.save = false;
    params.mode = static_cast<OriginMode>(response.payload[0]);
    params.direction = static_cast<Direction>(response.payload[1]);
    params.speed_rpm = LoadBE16(response.payload.data() + 2);
    params.timeout_ms = LoadBE32(response.payload.data() + 4);
    params.collision_speed_rpm = LoadBE16(response.payload.data() + 8);
    params.collision_current_ma = LoadBE16(response.payload.data() + 10);
    params.collision_time_ms = LoadBE16(response.payload.data() + 12);
    params.auto_return_on_power = response.payload[14] != 0;
    return IsValidOriginParams(params) ? LibXR::ErrorCode::OK
                                       : LibXR::ErrorCode::OUT_OF_RANGE;
  }

  static LibXR::ErrorCode DecodePIDParams(const Response &response,
                                          PIDParams &params) {
    const auto result = ValidateDecode(response, 0x21, 12);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    params.kp = LoadBE32(response.payload.data());
    params.ki = LoadBE32(response.payload.data() + 4);
    params.kd = LoadBE32(response.payload.data() + 8);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeDMX512Params(const Response &response,
                                             Dmx512Params &params) {
    const auto result = ValidateDecode(response, 0x49, 14);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    params.save = false;
    params.start_channel = LoadBE16(response.payload.data());
    params.channel_count = response.payload[2];
    params.mode = static_cast<Dmx512MotionMode>(response.payload[3]);
    params.speed_rpm = LoadBE16(response.payload.data() + 4);
    params.acceleration_step = LoadBE16(response.payload.data() + 6);
    params.velocity_step_rpm = LoadBE16(response.payload.data() + 8);
    params.position_step = LoadBE32(response.payload.data() + 10);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeProtectionParams(
      const Response &response, ProtectionParams &params) {
    const auto result = ValidateDecode(response, 0x13, 6);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    params.save = false;
    params.over_temperature_c = LoadBE16(response.payload.data());
    params.over_current_ma = LoadBE16(response.payload.data() + 2);
    params.trigger_time_ms = LoadBE16(response.payload.data() + 4);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeEmmSystemState(
      const Response &response, EmmSystemState &state) {
    const auto result = ValidateDecode(response, 0x43, 28);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    state.total_bytes = response.payload[0];
    state.parameter_count = response.payload[1];
    state.bus_voltage_mv = LoadBE16(response.payload.data() + 2);
    state.phase_current_ma = LoadBE16(response.payload.data() + 4);
    state.encoder_linearized = LoadBE16(response.payload.data() + 6);
    state.target_position.negative = response.payload[8] != 0;
    state.target_position.magnitude = LoadBE32(response.payload.data() + 9);
    state.speed.negative = response.payload[13] != 0;
    state.speed.rpm = LoadBE16(response.payload.data() + 14);
    state.position.negative = response.payload[16] != 0;
    state.position.magnitude = LoadBE32(response.payload.data() + 17);
    state.position_error.negative = response.payload[21] != 0;
    state.position_error.magnitude = LoadBE32(response.payload.data() + 22);
    state.origin_flags = ParseOriginStatusFlags(response.payload[26]);
    state.motor_flags = ParseMotorStatusFlags(response.payload[27]);
    return LibXR::ErrorCode::OK;
  }

  static LibXR::ErrorCode DecodeEmmMotorConfig(
      const Response &response, EmmMotorConfig &config) {
    const auto result = ValidateDecode(response, 0x42, 30);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    config.total_bytes = response.payload[0];
    config.parameter_count = response.payload[1];
    config.motor_type = static_cast<MotorType>(response.payload[2]);
    config.pulse_port_mode =
        static_cast<PulsePortMode>(response.payload[3]);
    config.communication_port_mode =
        static_cast<CommunicationPortMode>(response.payload[4]);
    config.enable_pin_level =
        static_cast<EnablePinLevel>(response.payload[5]);
    config.positive_direction =
        static_cast<Direction>(response.payload[6]);
    config.microstep = response.payload[7];
    config.interpolation = response.payload[8] != 0;
    config.reserved = response.payload[9];
    config.open_loop_current_ma = LoadBE16(response.payload.data() + 10);
    config.closed_loop_current_ma = LoadBE16(response.payload.data() + 12);
    config.closed_loop_max_voltage = LoadBE16(response.payload.data() + 14);
    config.serial_baud_rate =
        static_cast<SerialBaudRate>(response.payload[16]);
    config.can_bitrate = static_cast<CanBitrate>(response.payload[17]);
    config.motor_id = response.payload[18];
    config.communication_protocol =
        static_cast<CommunicationProtocol>(response.payload[19]);
    config.response_mode =
        static_cast<ControlResponseMode>(response.payload[20]);
    config.clog_protection =
        static_cast<ClogProtectionMode>(response.payload[21]);
    config.clog_speed_rpm = LoadBE16(response.payload.data() + 22);
    config.clog_current_ma = LoadBE16(response.payload.data() + 24);
    config.clog_time_ms = LoadBE16(response.payload.data() + 26);
    config.position_window_tenths_degree =
        LoadBE16(response.payload.data() + 28);
    return IsValidMotorConfig(config) ? LibXR::ErrorCode::OK
                                      : LibXR::ErrorCode::OUT_OF_RANGE;
  }

  static LibXR::ErrorCode DecodeBroadcastMotorID(const Response &response,
                                                  uint8_t &motor_id) {
    const auto result = ValidateDecode(response, 0x15, 1);
    if (result != LibXR::ErrorCode::OK) {
      return result;
    }
    motor_id = response.payload[0];
    return (motor_id == BROADCAST_ID) ? LibXR::ErrorCode::OUT_OF_RANGE
                                      : LibXR::ErrorCode::OK;
  }

  template <size_t CAPACITY>
  LibXR::ErrorCode SendBatch(const CommandBatch<CAPACITY> &batch,
                             uint8_t batch_addr = BROADCAST_ID) {
    if (batch.Size() == 0) {
      return LibXR::ErrorCode::EMPTY;
    }
    if (batch.GetChecksumMode() != checksum_mode_) {
      return LibXR::ErrorCode::ARG_ERR;
    }

    const size_t wrapped_size = batch.Size() + 5U;
    if (wrapped_size > 0xFFFFU) {
      return LibXR::ErrorCode::SIZE_ERR;
    }

    std::array<uint8_t, CAPACITY + 2U> payload{};
    payload[0] = static_cast<uint8_t>(wrapped_size >> 8U);
    payload[1] = static_cast<uint8_t>(wrapped_size);
    for (size_t index = 0; index < batch.Size(); ++index) {
      payload[index + 2U] = batch.Data()[index];
    }
    return SendRaw(batch_addr, 0xAA, payload.data(), batch.Size() + 2U);
  }

  LibXR::ErrorCode SendRaw(uint8_t motor_id, uint8_t command,
                           const uint8_t *payload, size_t payload_len,
                           bool append_checksum = true) {
    if (payload == nullptr && payload_len != 0) {
      return LibXR::ErrorCode::PTR_NULL;
    }

    const size_t virtual_payload_len =
        payload_len + (append_checksum ? 1U : 0U);
    if (virtual_payload_len > 7U * 256U) {
      return LibXR::ErrorCode::SIZE_ERR;
    }

    const uint8_t checksum =
        CalculateChecksum(checksum_mode_, motor_id, command, payload,
                          payload_len);
    if (virtual_payload_len == 0) {
      LibXR::CAN::ClassicPack pack{};
      pack.id = MakeExtID(motor_id, 0);
      pack.type = LibXR::CAN::Type::EXTENDED;
      pack.dlc = 1;
      pack.data[0] = command;
      return can_->AddMessage(pack);
    }

    size_t offset = 0;
    size_t packet_index = 0;
    while (offset < virtual_payload_len) {
      LibXR::CAN::ClassicPack pack{};
      pack.id = MakeExtID(motor_id, static_cast<uint8_t>(packet_index));
      pack.type = LibXR::CAN::Type::EXTENDED;
      pack.data[0] = command;

      const size_t remaining = virtual_payload_len - offset;
      const size_t chunk = (remaining > 7U) ? 7U : remaining;
      for (size_t index = 0; index < chunk; ++index) {
        const size_t source_index = offset + index;
        pack.data[index + 1U] =
            (source_index < payload_len) ? payload[source_index] : checksum;
      }
      pack.dlc = static_cast<uint8_t>(chunk + 1U);

      const auto result = can_->AddMessage(pack);
      if (result != LibXR::ErrorCode::OK) {
        return result;
      }
      offset += chunk;
      ++packet_index;
    }
    return LibXR::ErrorCode::OK;
  }

private:
  struct RxAssembly {
    uint8_t motor_id = 0;
    uint8_t command = 0;
    uint8_t next_packet = 0;
    uint8_t frame_count = 0;
    size_t size = 0;
    std::array<uint8_t, MAX_RESPONSE_BYTES + 1U> bytes{};
    bool active = false;
  };

  LibXR::ErrorCode SendCommand(uint8_t command, const uint8_t *payload,
                               size_t payload_len) {
    return SendRaw(motor_id_, command, payload, payload_len);
  }

  static void OnCanMessage(bool in_isr, ZDTMotor *self,
                           const LibXR::CAN::ClassicPack &pack) {
    if (pack.type != LibXR::CAN::Type::EXTENDED || pack.dlc == 0 ||
        pack.dlc > 8 || (pack.id & 0x1FFF0000UL) != 0) {
      return;
    }

    const uint8_t rx_motor_id =
        static_cast<uint8_t>((pack.id >> 8U) & 0xFFU);
    const uint8_t packet_index = static_cast<uint8_t>(pack.id & 0xFFU);
    const uint8_t command = pack.data[0];
    const bool accepts_motor = self->receive_all_motor_ids_ ||
                               self->motor_id_ == BROADCAST_ID ||
                               rx_motor_id == self->motor_id_ ||
                               command == 0x15;
    if (!accepts_motor) {
      return;
    }

    ++self->rx_count_;
    RxAssembly *assembly = nullptr;
    if (packet_index == 0) {
      assembly = self->StartAssembly(rx_motor_id, command);
    } else {
      assembly = self->FindAssembly(rx_motor_id, command);
      if (assembly == nullptr || assembly->next_packet != packet_index) {
        ++self->sequence_error_count_;
        if (assembly != nullptr) {
          assembly->active = false;
        }
        return;
      }
    }

    const size_t chunk_size = static_cast<size_t>(pack.dlc - 1U);
    if (assembly->size + chunk_size > assembly->bytes.size()) {
      ++self->overflow_count_;
      assembly->active = false;
      return;
    }
    for (size_t index = 0; index < chunk_size; ++index) {
      assembly->bytes[assembly->size++] = pack.data[index + 1U];
    }
    assembly->frame_count = static_cast<uint8_t>(assembly->frame_count + 1U);
    assembly->next_packet = static_cast<uint8_t>(packet_index + 1U);

    if (self->AssemblyComplete(*assembly, pack.dlc)) {
      self->FinishAssembly(*assembly, packet_index, in_isr);
    }
  }

  RxAssembly *StartAssembly(uint8_t motor_id, uint8_t command) {
    RxAssembly *assembly = FindAssembly(motor_id, command);
    if (assembly == nullptr) {
      for (auto &candidate : rx_assemblies_) {
        if (!candidate.active) {
          assembly = &candidate;
          break;
        }
      }
    }
    if (assembly == nullptr) {
      assembly = &rx_assemblies_[rx_slot_cursor_];
      rx_slot_cursor_ = (rx_slot_cursor_ + 1U) % rx_assemblies_.size();
      ++overflow_count_;
    }

    *assembly = RxAssembly{};
    assembly->motor_id = motor_id;
    assembly->command = command;
    assembly->active = true;
    return assembly;
  }

  RxAssembly *FindAssembly(uint8_t motor_id, uint8_t command) {
    for (auto &assembly : rx_assemblies_) {
      if (assembly.active && assembly.motor_id == motor_id &&
          assembly.command == command) {
        return &assembly;
      }
    }
    return nullptr;
  }

  void ResetAssemblies() {
    for (auto &assembly : rx_assemblies_) {
      assembly.active = false;
    }
  }

  bool AssemblyComplete(const RxAssembly &assembly, uint8_t dlc) const {
    const size_t expected = ExpectedResponseWireLength(assembly);
    if (expected != 0 && assembly.size >= expected) {
      return true;
    }
    if (dlc < 8) {
      return true;
    }
    return expected == 0 && CandidateChecksumValid(assembly);
  }

  bool CandidateChecksumValid(const RxAssembly &assembly) const {
    if (assembly.size == 0) {
      return false;
    }
    const size_t data_size = assembly.size - 1U;
    const uint8_t expected = CalculateChecksum(
        checksum_mode_, assembly.motor_id, assembly.command,
        assembly.bytes.data(), data_size);
    return expected == assembly.bytes[data_size];
  }

  void FinishAssembly(RxAssembly &assembly, uint8_t packet_index,
                      bool in_isr) {
    Response response{};
    response.motor_id = assembly.motor_id;
    response.packet_index = packet_index;
    response.command = assembly.command;
    response.frame_count = assembly.frame_count;

    if (assembly.size != 0) {
      const size_t data_size = assembly.size - 1U;
      response.length = static_cast<uint8_t>(
          (data_size > MAX_RESPONSE_BYTES) ? MAX_RESPONSE_BYTES : data_size);
      for (size_t index = 0; index < response.length; ++index) {
        response.payload[index] = assembly.bytes[index];
      }
      response.checksum = assembly.bytes[data_size];
      response.checksum_valid = CandidateChecksumValid(assembly);
      response.valid = response.checksum_valid;
      if (response.length == 1) {
        response.status = ClassifyStatus(
            response.command, response.payload[0], response.length);
      }
    }

    if (!response.checksum_valid) {
      ++checksum_error_count_;
    }
    ++response_count_;
    last_response_ = response;
    assembly.active = false;
    response_callback_.Run(in_isr, last_response_);
  }

  static size_t ExpectedResponseWireLength(const RxAssembly &assembly) {
    switch (assembly.command) {
    case 0x13:
      return 7;
    case 0x15:
      return 2;
    case 0x16:
      return 5;
    case 0x1A:
      return 0;
    case 0x1F:
    case 0x20:
      return 5;
    case 0x21:
      return 13;
    case 0x22:
      return 16;
    case 0x23:
      return 5;
    case 0x24:
    case 0x26:
    case 0x27:
    case 0x29:
    case 0x31:
    case 0x38:
    case 0x3F:
    case 0x41:
      return 3;
    case 0x30:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x36:
    case 0x37:
      return 6;
    case 0x35:
      return 4;
    case 0x39:
    case 0x3C:
      return 3;
    case 0x3A:
    case 0x3B:
    case 0x3D:
      return 2;
    case 0x42:
    case 0x43:
      if (assembly.size != 0 && assembly.bytes[0] >= 3) {
        const size_t expected = static_cast<size_t>(assembly.bytes[0] - 2U);
        return (expected <= assembly.bytes.size()) ? expected : 0;
      }
      return 0;
    case 0x49:
      return 15;
    case 0x11:
    case 0xAA:
      return 0;
    default:
      return 2;
    }
  }

  static constexpr bool IsReadCommand(uint8_t command) {
    switch (command) {
    case 0x13:
    case 0x15:
    case 0x16:
    case 0x1A:
    case 0x1F:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x26:
    case 0x27:
    case 0x29:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3F:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x49:
      return true;
    default:
      return false;
    }
  }

  static constexpr ResponseStatus ClassifyStatus(uint8_t command,
                                                  uint8_t value,
                                                  size_t length) {
    if (length != 1) {
      return ResponseStatus::NONE;
    }
    switch (value) {
    case 0xE2:
      return ResponseStatus::PARAMETER_ERROR;
    case 0xEE:
      return ResponseStatus::FORMAT_ERROR;
    case 0x9F:
      return ResponseStatus::COMPLETE;
    case 0x12:
      return (command == 0x9A) ? ResponseStatus::ALREADY_AT_ORIGIN
                               : ResponseStatus::NONE;
    case 0x02:
      return IsReadCommand(command) ? ResponseStatus::NONE
                                    : ResponseStatus::OK;
    default:
      return ResponseStatus::NONE;
    }
  }

  static LibXR::ErrorCode ValidateDecode(const Response &response,
                                         uint8_t expected_command,
                                         size_t expected_length) {
    return ValidateDecodeRange(response, expected_command, expected_length,
                               expected_length);
  }

  static LibXR::ErrorCode ValidateDecodeRange(const Response &response,
                                              uint8_t expected_command,
                                              size_t minimum_length,
                                              size_t maximum_length) {
    if (!response.valid) {
      return (response.frame_count == 0) ? LibXR::ErrorCode::NO_RESPONSE
                                         : LibXR::ErrorCode::CHECK_ERR;
    }
    if (response.command != expected_command) {
      return LibXR::ErrorCode::NOT_FOUND;
    }
    if (response.IsError()) {
      return LibXR::ErrorCode::FAILED;
    }
    if (response.length < minimum_length ||
        response.length > maximum_length) {
      return LibXR::ErrorCode::SIZE_ERR;
    }
    return LibXR::ErrorCode::OK;
  }

  static constexpr uint16_t LoadBE16(const uint8_t *data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                 static_cast<uint16_t>(data[1]));
  }

  static constexpr uint32_t LoadBE32(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 24U) |
           (static_cast<uint32_t>(data[1]) << 16U) |
           (static_cast<uint32_t>(data[2]) << 8U) |
           static_cast<uint32_t>(data[3]);
  }

  static constexpr void StoreBE16(uint8_t *data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value >> 8U);
    data[1] = static_cast<uint8_t>(value);
  }

  static constexpr void StoreBE32(uint8_t *data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value >> 24U);
    data[1] = static_cast<uint8_t>(value >> 16U);
    data[2] = static_cast<uint8_t>(value >> 8U);
    data[3] = static_cast<uint8_t>(value);
  }

  static constexpr uint8_t UpdateCRC8(uint8_t checksum, uint8_t data) {
    uint8_t index = static_cast<uint8_t>(checksum ^ data);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      index = (index & 0x01U) != 0
                  ? static_cast<uint8_t>((index >> 1U) ^ 0x8CU)
                  : static_cast<uint8_t>(index >> 1U);
    }
    return index;
  }

  static constexpr ChecksumMode DecodeChecksumMode(uint8_t checksum_mode) {
    return (checksum_mode <= static_cast<uint8_t>(ChecksumMode::CRC8))
               ? static_cast<ChecksumMode>(checksum_mode)
               : ChecksumMode::FIXED_6B;
  }

  static constexpr bool IsValidSpeed(uint16_t speed_rpm) {
    return speed_rpm <= MAX_EMM_SPEED_RPM;
  }

  static constexpr bool IsValidMotionMode(MotionMode mode) {
    return static_cast<uint8_t>(mode) <=
           static_cast<uint8_t>(MotionMode::RELATIVE_TO_CURRENT);
  }

  static constexpr bool IsValidOriginMode(OriginMode mode) {
    return static_cast<uint8_t>(mode) <=
           static_cast<uint8_t>(OriginMode::POWER_LOSS_POSITION);
  }

  static constexpr bool IsValidOriginParams(const OriginParams &params) {
    return IsValidOriginMode(params.mode) && IsValidSpeed(params.speed_rpm) &&
           IsValidSpeed(params.collision_speed_rpm) &&
           params.collision_current_ma <= MAX_CURRENT_MA;
  }

  static constexpr bool IsValidMotorConfig(const EmmMotorConfig &config) {
    const uint8_t motor_type = static_cast<uint8_t>(config.motor_type);
    return (motor_type == static_cast<uint8_t>(MotorType::STEP_0_9_DEG) ||
            motor_type == static_cast<uint8_t>(MotorType::STEP_1_8_DEG)) &&
           static_cast<uint8_t>(config.pulse_port_mode) <= 4 &&
           static_cast<uint8_t>(config.communication_port_mode) <= 4 &&
           static_cast<uint8_t>(config.enable_pin_level) <= 2 &&
           config.open_loop_current_ma <= MAX_CURRENT_MA &&
           config.closed_loop_current_ma <= MAX_CURRENT_MA &&
           config.closed_loop_max_voltage <= MAX_CURRENT_MA &&
           static_cast<uint8_t>(config.serial_baud_rate) <= 8 &&
           static_cast<uint8_t>(config.can_bitrate) <= 9 &&
           config.motor_id != BROADCAST_ID &&
           static_cast<uint8_t>(config.communication_protocol) <= 4 &&
           static_cast<uint8_t>(config.response_mode) <= 4 &&
           static_cast<uint8_t>(config.clog_protection) <= 2 &&
           IsValidSpeed(config.clog_speed_rpm) &&
           config.clog_current_ma <= MAX_CURRENT_MA;
  }

  static constexpr float ClampDuty(float duty) {
    if (duty > 1.0F) {
      return 1.0F;
    }
    if (duty < -1.0F) {
      return -1.0F;
    }
    return duty;
  }

  static constexpr Direction DirectionFromSigned(float value) {
    return (value >= 0.0F) ? Direction::CW : Direction::CCW;
  }

  static uint16_t DutyToSpeedRPM(float duty, uint16_t max_speed_rpm) {
    const float speed =
        std::abs(ClampDuty(duty)) * static_cast<float>(max_speed_rpm);
    if (speed <= 0.0F) {
      return 0;
    }
    if (speed >= static_cast<float>(MAX_EMM_SPEED_RPM)) {
      return MAX_EMM_SPEED_RPM;
    }
    return static_cast<uint16_t>(speed + 0.5F);
  }

  static uint32_t TargetDeltaToPulseCount(float target_delta,
                                          float pulses_per_unit) {
    const float pulses = std::abs(target_delta) * pulses_per_unit;
    if (pulses <= 0.0F) {
      return 0;
    }
    if (pulses >= 4294967295.0F) {
      return 0xFFFFFFFFUL;
    }
    return static_cast<uint32_t>(pulses + 0.5F);
  }

  static constexpr int32_t SignedPulseDelta(float target_delta,
                                            uint32_t pulse_count) {
    constexpr int32_t POSITIVE_LIMIT = 2147483647;
    constexpr int32_t NEGATIVE_LIMIT = (-2147483647 - 1);
    if (target_delta >= 0.0F) {
      return (pulse_count > static_cast<uint32_t>(POSITIVE_LIMIT))
                 ? POSITIVE_LIMIT
                 : static_cast<int32_t>(pulse_count);
    }
    return (pulse_count > static_cast<uint32_t>(POSITIVE_LIMIT))
               ? NEGATIVE_LIMIT
               : -static_cast<int32_t>(pulse_count);
  }

  static constexpr int32_t SaturatingAdd(int32_t lhs, int32_t rhs) {
    constexpr int64_t POSITIVE_LIMIT = 2147483647LL;
    constexpr int64_t NEGATIVE_LIMIT = -2147483648LL;
    const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
    if (sum > POSITIVE_LIMIT) {
      return static_cast<int32_t>(POSITIVE_LIMIT);
    }
    if (sum < NEGATIVE_LIMIT) {
      return static_cast<int32_t>(NEGATIVE_LIMIT);
    }
    return static_cast<int32_t>(sum);
  }

  LibXR::CAN *can_;
  uint8_t motor_id_;
  ChecksumMode checksum_mode_ = ChecksumMode::FIXED_6B;
  bool receive_all_motor_ids_ = false;
  BusinessFeedback feedback_{};
  bool open_loop_update_pending_ = false;
  float pulses_per_unit_;
  uint16_t default_speed_rpm_;
  uint8_t default_acceleration_;
  uint16_t duty_max_speed_rpm_;
  Response last_response_{};
  uint32_t rx_count_ = 0;
  uint32_t response_count_ = 0;
  uint32_t checksum_error_count_ = 0;
  uint32_t sequence_error_count_ = 0;
  uint32_t overflow_count_ = 0;
  std::array<RxAssembly, RX_ASSEMBLY_SLOTS> rx_assemblies_{};
  size_t rx_slot_cursor_ = 0;
  LibXR::CAN::ErrorState can_error_state_{};
  ResponseCallback response_callback_{};
  LibXR::CAN::Callback rx_callback_;
};
