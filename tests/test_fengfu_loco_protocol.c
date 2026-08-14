#include "fengfu_loco_protocol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int require_true(int condition, const char* message) {
    if (condition) return 1;
    fprintf(stderr, "FAIL: %s\n", message);
    return 0;
}

int main(void) {
    FF_LocoCmd command;
    FF_LocoCmd decoded_command;
    uint8_t command_payload[FF_TCP_LOCO_CMD_PAYLOAD_SIZE];
    memset(&command, 0, sizeof(command));
    command.vx = 0.25;
    command.vy = -0.10;
    command.wz = 0.15;
    command.duration_ms = 1500;
    ff_tcp_encode_loco_cmd(command_payload, &command);
    if (!require_true(ff_tcp_decode_loco_cmd(command_payload, &decoded_command) == FF_OK,
                      "loco command decode")) return 1;
    if (!require_true(fabs(decoded_command.vx - 0.25) < 1e-12 &&
                          fabs(decoded_command.vy + 0.10) < 1e-12 &&
                          fabs(decoded_command.wz - 0.15) < 1e-12 &&
                          decoded_command.duration_ms == 1500,
                      "loco command round trip")) return 1;

    FF_LocoState state;
    FF_LocoState decoded_state;
    uint8_t state_payload[FF_TCP_LOCO_STATE_PAYLOAD_SIZE];
    memset(&state, 0, sizeof(state));
    state.timestamp_ns = 123456789;
    state.acknowledged_seq = 42;
    state.current_mode = FF_LOCO_MODE_RL;
    state.hardware_rl_enabled = 1;
    state.accepted = 1;
    state.runtime_health = FF_RUNTIME_READY;
    state.remaining_ms = 900;
    state.applied_vx = 0.20;
    ff_tcp_encode_loco_state(state_payload, &state);
    if (!require_true(ff_tcp_decode_loco_state(state_payload, &decoded_state) == FF_OK,
                      "loco state decode")) return 1;
    if (!require_true(decoded_state.acknowledged_seq == 42 &&
                          decoded_state.current_mode == FF_LOCO_MODE_RL &&
                          decoded_state.runtime_health == FF_RUNTIME_READY &&
                          fabs(decoded_state.applied_vx - 0.20) < 1e-12,
                      "loco state round trip")) return 1;

    FF_MotorState motors;
    FF_MotorState decoded_motors;
    uint8_t motor_payload[FF_TCP_MOTOR_STATE_PAYLOAD_SIZE];
    memset(&motors, 0, sizeof(motors));
    motors.sequence_id = 88;
    motors.state_valid = 1;
    motors.active_control_mode = FF_LOW_LEVEL_CONTROL_POSITION_PD;
    motors.command_fresh = 1;
    motors.q[7] = -1.23;
    motors.motor_valid[7] = 1;
    motors.foot_force_raw[7] = 777;
    motors.port_transaction_ok[2] = 1;
    ff_tcp_encode_motor_state(motor_payload, &motors);
    if (!require_true(ff_tcp_decode_motor_state(motor_payload, &decoded_motors) == FF_OK,
                      "motor state decode")) return 1;
    if (!require_true(decoded_motors.sequence_id == 88 &&
                          decoded_motors.state_valid == 1 &&
                          fabs(decoded_motors.q[7] + 1.23) < 1e-12 &&
                          decoded_motors.foot_force_raw[7] == 777 &&
                          decoded_motors.port_transaction_ok[2] == 1,
                      "motor state round trip")) return 1;

    FF_TcpHeader header;
    FF_TcpHeader decoded_header;
    uint8_t header_bytes[FF_TCP_HEADER_SIZE];
    memset(&header, 0, sizeof(header));
    header.magic = FF_TCP_MAGIC;
    header.version = FF_TCP_VERSION;
    header.msg_type = FF_TCP_MSG_LOCO_CMD;
    header.length = FF_TCP_LOCO_CMD_PAYLOAD_SIZE;
    header.seq = 9;
    header.crc32 = ff_tcp_crc32(command_payload, sizeof(command_payload));
    ff_tcp_encode_header(header_bytes, &header);
    if (!require_true(ff_tcp_decode_header(header_bytes, &decoded_header) == FF_OK &&
                          ff_tcp_validate_header(&decoded_header, FF_TCP_MSG_LOCO_CMD,
                                                 FF_TCP_LOCO_CMD_PAYLOAD_SIZE,
                                                 command_payload) == FF_OK,
                      "header and CRC validation")) return 1;

    puts("fengfu standalone loco protocol tests passed");
    return 0;
}

