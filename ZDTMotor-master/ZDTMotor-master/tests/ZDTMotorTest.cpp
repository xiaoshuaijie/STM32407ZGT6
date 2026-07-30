#include <cstdlib>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <vector>

#include "ZDTMotor.hpp"

extern "C" void libxr_fatal_error(const char *, uint32_t, bool) {
  std::abort();
}

namespace {

int failure_count = 0;

void Check(bool condition, const char *expression, int line) {
  if (!condition) {
    std::cerr << "line " << line << ": check failed: " << expression << '\n';
    ++failure_count;
  }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

class MockCAN : public LibXR::CAN {
public:
  LibXR::ErrorCode SetConfig(const Configuration &configuration) override {
    configuration_ = configuration;
    return LibXR::ErrorCode::OK;
  }

  uint32_t GetClockFreq() const override { return 80000000; }

  LibXR::ErrorCode AddMessage(const ClassicPack &pack) override {
    transmitted.push_back(pack);
    return LibXR::ErrorCode::OK;
  }

  void Inject(uint8_t motor_id, uint8_t packet_index,
              std::initializer_list<uint8_t> data) {
    ClassicPack pack{};
    pack.id = ZDTMotor::MakeExtID(motor_id, packet_index);
    pack.type = Type::EXTENDED;
    pack.dlc = static_cast<uint8_t>(data.size());
    size_t index = 0;
    for (uint8_t value : data) {
      pack.data[index++] = value;
    }
    OnMessage(pack, false);
  }

  void InjectResponse(uint8_t motor_id, uint8_t command,
                      const std::vector<uint8_t> &wire_payload) {
    size_t offset = 0;
    uint8_t packet_index = 0;
    while (offset < wire_payload.size()) {
      ClassicPack pack{};
      pack.id = ZDTMotor::MakeExtID(motor_id, packet_index);
      pack.type = Type::EXTENDED;
      pack.data[0] = command;
      const size_t remaining = wire_payload.size() - offset;
      const size_t chunk = (remaining > 7U) ? 7U : remaining;
      for (size_t index = 0; index < chunk; ++index) {
        pack.data[index + 1U] = wire_payload[offset + index];
      }
      pack.dlc = static_cast<uint8_t>(chunk + 1U);
      OnMessage(pack, false);
      offset += chunk;
      ++packet_index;
    }
  }

  void Clear() { transmitted.clear(); }

  std::vector<ClassicPack> transmitted;

private:
  Configuration configuration_{};
};

void ExpectFrame(const MockCAN &can, size_t frame_index, uint32_t id,
                 std::initializer_list<uint8_t> data) {
  CHECK(frame_index < can.transmitted.size());
  if (frame_index >= can.transmitted.size()) {
    return;
  }
  const auto &frame = can.transmitted[frame_index];
  CHECK(frame.id == id);
  CHECK(frame.type == LibXR::CAN::Type::EXTENDED);
  CHECK(frame.dlc == data.size());
  size_t data_index = 0;
  for (uint8_t value : data) {
    CHECK(frame.data[data_index] == value);
    ++data_index;
  }
}

void TestDocumentedPositionFrames(ZDTMotor &motor, MockCAN &can) {
  can.Clear();
  CHECK(motor.Position(ZDTMotor::Direction::CCW, 1500, 0, 32000,
                       ZDTMotor::MotionMode::RELATIVE_TO_LAST_TARGET,
                       false) == LibXR::ErrorCode::OK);
  CHECK(can.transmitted.size() == 2);
  ExpectFrame(can, 0, 0x0100,
              {0xFD, 0x01, 0x05, 0xDC, 0x00, 0x00, 0x00, 0x7D});
  ExpectFrame(can, 1, 0x0101, {0xFD, 0x00, 0x00, 0x00, 0x6B});
}

void TestFastPositionFrames(ZDTMotor &motor, MockCAN &can) {
  can.Clear();
  ZDTMotor::FastPositionParams params{};
  params.speed_rpm = 800;
  params.acceleration = 100;
  CHECK(motor.ConfigureFastPosition(params) == LibXR::ErrorCode::OK);
  CHECK(motor.FastPosition(3200) == LibXR::ErrorCode::OK);
  CHECK(can.transmitted.size() == 2);
  ExpectFrame(can, 0, 0x0100,
              {0xF1, 0x03, 0x20, 0x64, 0x00, 0x00, 0x6B});
  ExpectFrame(can, 1, 0x0100, {0xFC, 0x00, 0x00, 0x0C, 0x80, 0x6B});
}

void TestChecksums(ZDTMotor &motor, MockCAN &can) {
  motor.SetChecksumMode(ZDTMotor::ChecksumMode::XOR);
  can.Clear();
  CHECK(motor.TriggerEncoderCalibration() == LibXR::ErrorCode::OK);
  ExpectFrame(can, 0, 0x0100, {0x06, 0x45, 0x42});

  motor.SetChecksumMode(ZDTMotor::ChecksumMode::CRC8);
  can.Clear();
  CHECK(motor.TriggerEncoderCalibration() == LibXR::ErrorCode::OK);
  ExpectFrame(can, 0, 0x0100, {0x06, 0x45, 0x17});

  motor.SetChecksumMode(ZDTMotor::ChecksumMode::FIXED_6B);
}

void TestDocumentedMultiMotorFrames(ZDTMotor &motor, MockCAN &can) {
  ZDTMotor::CommandBatch<> batch;
  CHECK(batch.AppendPosition(
            2, ZDTMotor::Direction::CCW, 1500, 8, 32000,
            ZDTMotor::MotionMode::RELATIVE_TO_LAST_TARGET, false) ==
        LibXR::ErrorCode::OK);
  CHECK(batch.AppendPosition(3, ZDTMotor::Direction::CW, 1000, 10, 64000,
                             ZDTMotor::MotionMode::ABSOLUTE, true) ==
        LibXR::ErrorCode::OK);
  CHECK(batch.AppendReadSystemParam(4, ZDTMotor::SystemParam::POSITION) ==
        LibXR::ErrorCode::OK);

  can.Clear();
  CHECK(motor.SendBatch(batch) == LibXR::ErrorCode::OK);
  CHECK(can.transmitted.size() == 5);
  ExpectFrame(can, 0, 0x0000,
              {0xAA, 0x00, 0x22, 0x02, 0xFD, 0x01, 0x05, 0xDC});
  ExpectFrame(can, 1, 0x0001,
              {0xAA, 0x08, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00});
  ExpectFrame(can, 2, 0x0002,
              {0xAA, 0x6B, 0x03, 0xFD, 0x00, 0x03, 0xE8, 0x0A});
  ExpectFrame(can, 3, 0x0003,
              {0xAA, 0x00, 0x00, 0xFA, 0x00, 0x01, 0x01, 0x6B});
  ExpectFrame(can, 4, 0x0004, {0xAA, 0x04, 0x36, 0x6B, 0x6B});
}

void TestSecondGenerationCommands(ZDTMotor &motor, MockCAN &can) {
  can.Clear();
  CHECK(motor.ModifyControlMode(
            true, ZDTMotor::ControlMode::CLOSED_LOOP_FOC) ==
        LibXR::ErrorCode::OK);
  CHECK(motor.ModifyFirmwareType(true, ZDTMotor::FirmwareType::EMM_TURBO) ==
        LibXR::ErrorCode::OK);
  CHECK(motor.ModifyCollisionOriginReturnAngle(true, 20) ==
        LibXR::ErrorCode::OK);
  CHECK(motor.BroadcastReadMotorID() == LibXR::ErrorCode::OK);
  CHECK(motor.ModifyParameterLock(
            true, ZDTMotor::ParameterLockLevel::COMMUNICATION_LOCKED) ==
        LibXR::ErrorCode::OK);
  CHECK(can.transmitted.size() == 5);
  ExpectFrame(can, 0, 0x0100, {0x46, 0xA6, 0x01, 0x01, 0x6B});
  ExpectFrame(can, 1, 0x0100, {0xD5, 0x69, 0x01, 0x02, 0x6B});
  ExpectFrame(can, 2, 0x0100,
              {0x5C, 0xAC, 0x01, 0x00, 0x14, 0x6B});
  ExpectFrame(can, 3, 0x0000, {0x15, 0x6B});
  ExpectFrame(can, 4, 0x0100, {0xD6, 0x4B, 0x01, 0x01, 0x6B});
}

void TestDocumentedMotorConfigWrite(ZDTMotor &motor, MockCAN &can) {
  ZDTMotor::EmmMotorConfig config{};
  config.motor_type = ZDTMotor::MotorType::STEP_0_9_DEG;
  config.closed_loop_current_ma = 3200;
  config.clog_current_ma = 3000;
  config.position_window_tenths_degree = 1;

  can.Clear();
  CHECK(motor.ModifyMotorConfigParams(true, config) ==
        LibXR::ErrorCode::OK);
  CHECK(can.transmitted.size() == 5);
  ExpectFrame(can, 0, 0x0100,
              {0x48, 0xD1, 0x01, 0x19, 0x02, 0x02, 0x02, 0x00});
  ExpectFrame(can, 1, 0x0101,
              {0x48, 0x10, 0x01, 0x00, 0x04, 0xB0, 0x0C, 0x80});
  ExpectFrame(can, 2, 0x0102,
              {0x48, 0x0F, 0xA0, 0x05, 0x07, 0x00, 0x00, 0x01});
  ExpectFrame(can, 3, 0x0103,
              {0x48, 0x01, 0x00, 0x08, 0x0B, 0xB8, 0x07, 0xD0});
  ExpectFrame(can, 4, 0x0104, {0x48, 0x00, 0x01, 0x6B});
}

void TestSystemStateReassembly(ZDTMotor &motor, MockCAN &can) {
  const std::vector<uint8_t> response = {
      0x1F, 0x09, 0x5C, 0x67, 0x00, 0x03, 0x43, 0xEB, 0x01, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x03, 0x03, 0x6B,
  };
  const uint32_t response_count = motor.ResponseCount();
  can.InjectResponse(1, 0x43, response);
  CHECK(motor.ResponseCount() == response_count + 1);

  const auto raw = motor.LastResponse();
  CHECK(raw.valid);
  CHECK(raw.checksum_valid);
  CHECK(raw.frame_count == 5);
  CHECK(raw.length == 28);

  ZDTMotor::EmmSystemState state{};
  CHECK(ZDTMotor::DecodeEmmSystemState(raw, state) == LibXR::ErrorCode::OK);
  CHECK(state.total_bytes == 0x1F);
  CHECK(state.parameter_count == 0x09);
  CHECK(state.bus_voltage_mv == 23655);
  CHECK(state.phase_current_ma == 3);
  CHECK(state.encoder_linearized == 17387);
  CHECK(state.target_position.Value() == -65536);
  CHECK(state.speed.Value() == 0);
  CHECK(state.position.Value() == 65536);
  CHECK(state.position_error.Value() == -8);
  CHECK(state.origin_flags.raw == 0x03);
  CHECK(state.motor_flags.raw == 0x03);
}

ZDTMotor::Response MakeReadResponse(
    uint8_t command, std::initializer_list<uint8_t> payload) {
  ZDTMotor::Response response{};
  response.command = command;
  response.length = static_cast<uint8_t>(payload.size());
  response.frame_count = 1;
  response.checksum_valid = true;
  response.valid = true;
  size_t index = 0;
  for (uint8_t value : payload) {
    response.payload[index++] = value;
  }
  return response;
}

void TestSignedReadDecoding() {
  ZDTMotor::SignedMagnitude32 position{};
  auto response = MakeReadResponse(0x36, {0x00, 0x00, 0x01, 0x00, 0x00});
  CHECK(ZDTMotor::DecodeSignedMagnitude32(response, 0x36, position) ==
        LibXR::ErrorCode::OK);
  CHECK(position.Value() == 65536);

  response = MakeReadResponse(0x36, {0x01, 0x00, 0x01, 0x00, 0x00});
  CHECK(ZDTMotor::DecodeSignedMagnitude32(response, 0x36, position) ==
        LibXR::ErrorCode::OK);
  CHECK(position.Value() == -65536);

  ZDTMotor::SignedSpeed speed{};
  response = MakeReadResponse(0x35, {0x00, 0x04, 0xB0});
  CHECK(ZDTMotor::DecodeSpeed(response, speed) == LibXR::ErrorCode::OK);
  CHECK(speed.Value() == 1200);

  response = MakeReadResponse(0x35, {0x01, 0x04, 0xB0});
  CHECK(ZDTMotor::DecodeSpeed(response, speed) == LibXR::ErrorCode::OK);
  CHECK(speed.Value() == -1200);
}

void TestTemperatureSignDecoding() {
  int16_t temperature_c = 0;
  auto response = MakeReadResponse(0x39, {0x00, 0x1B});
  CHECK(ZDTMotor::DecodeTemperature(response, temperature_c) ==
        LibXR::ErrorCode::OK);
  CHECK(temperature_c == 27);

  response = MakeReadResponse(0x39, {0x01, 0x05});
  CHECK(ZDTMotor::DecodeTemperature(response, temperature_c) ==
        LibXR::ErrorCode::OK);
  CHECK(temperature_c == -5);
}

void TestConfigDecode() {
  ZDTMotor::Response response{};
  response.command = 0x42;
  response.valid = true;
  response.checksum_valid = true;
  const uint8_t payload[] = {
      0x21, 0x15, 0x19, 0x02, 0x02, 0x02, 0x00, 0x10, 0x01, 0x00,
      0x04, 0xB0, 0x0B, 0x80, 0x0F, 0xA0, 0x05, 0x07, 0x01, 0x00,
      0x01, 0x01, 0x00, 0x08, 0x08, 0x98, 0x07, 0xD0, 0x00, 0x08,
  };
  response.length = sizeof(payload);
  for (size_t index = 0; index < sizeof(payload); ++index) {
    response.payload[index] = payload[index];
  }

  ZDTMotor::EmmMotorConfig config{};
  CHECK(ZDTMotor::DecodeEmmMotorConfig(response, config) ==
        LibXR::ErrorCode::OK);
  CHECK(config.motor_type == ZDTMotor::MotorType::STEP_0_9_DEG);
  CHECK(config.open_loop_current_ma == 1200);
  CHECK(config.closed_loop_current_ma == 2944);
  CHECK(config.closed_loop_max_voltage == 4000);
  CHECK(config.can_bitrate == ZDTMotor::CanBitrate::KBPS_500);
  CHECK(config.motor_id == 1);
  CHECK(config.clog_speed_rpm == 8);
  CHECK(config.clog_current_ma == 2200);
  CHECK(config.clog_time_ms == 2000);
  CHECK(config.position_window_tenths_degree == 8);
}

void TestChecksumFailure(ZDTMotor &motor, MockCAN &can) {
  const uint32_t checksum_errors = motor.ChecksumErrorCount();
  can.InjectResponse(1, 0x24, {0x12, 0x34, 0x00});
  const auto response = motor.LastResponse();
  CHECK(!response.valid);
  CHECK(!response.checksum_valid);
  CHECK(motor.ChecksumErrorCount() == checksum_errors + 1);
}

void TestOptionStatusCompatibility() {
  ZDTMotor::Response response{};
  response.command = 0x1A;
  response.length = 2;
  response.payload[0] = 0xB7;
  response.payload[1] = 0x03;
  response.checksum_valid = true;
  response.valid = true;

  ZDTMotor::OptionParamState state{};
  CHECK(ZDTMotor::DecodeOptionParamState(response, state) ==
        LibXR::ErrorCode::OK);
  CHECK(state.motor_type == ZDTMotor::MotorType::STEP_0_9_DEG);
  CHECK(state.firmware_type == ZDTMotor::FirmwareType::EMM);
  CHECK(state.control_mode == ZDTMotor::ControlMode::CLOSED_LOOP_FOC);
  CHECK(state.positive_direction == ZDTMotor::Direction::CCW);
  CHECK(state.button_locked);
  CHECK(state.input_scaled_by_ten);
  CHECK(state.parameter_lock ==
        ZDTMotor::ParameterLockLevel::ALL_PARAMETERS_AND_CALIBRATION_LOCKED);
}

void TestStatusDecode() {
  ZDTMotor::Response response{};
  response.command = 0xF6;
  response.length = 1;
  response.payload[0] = 0xE2;
  response.status = ZDTMotor::ResponseStatus::PARAMETER_ERROR;
  response.frame_count = 1;
  response.checksum_valid = true;
  response.valid = true;

  ZDTMotor::ResponseStatus status = ZDTMotor::ResponseStatus::NONE;
  CHECK(ZDTMotor::DecodeStatus(response, status) == LibXR::ErrorCode::OK);
  CHECK(status == ZDTMotor::ResponseStatus::PARAMETER_ERROR);
}

} // namespace

int main() {
  MockCAN can;
  LibXR::HardwareContainer hardware;
  hardware.Register(LibXR::Entry<LibXR::CAN>{can, {"can1"}});
  LibXR::ApplicationManager application_manager;
  ZDTMotor motor(hardware, application_manager);

  TestDocumentedPositionFrames(motor, can);
  TestFastPositionFrames(motor, can);
  TestChecksums(motor, can);
  TestDocumentedMultiMotorFrames(motor, can);
  TestSecondGenerationCommands(motor, can);
  TestDocumentedMotorConfigWrite(motor, can);
  TestSystemStateReassembly(motor, can);
  TestSignedReadDecoding();
  TestTemperatureSignDecoding();
  TestConfigDecode();
  TestChecksumFailure(motor, can);
  TestOptionStatusCompatibility();
  TestStatusDecode();

  if (failure_count != 0) {
    std::cerr << failure_count << " test checks failed\n";
    return 1;
  }
  std::cout << "ZDTMotor protocol tests passed\n";
  return 0;
}
