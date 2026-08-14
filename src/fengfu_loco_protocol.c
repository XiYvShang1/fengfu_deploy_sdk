#include "fengfu_loco_protocol.h"
#include "fengfu_loco_sdk.h"

#include <string.h>

static void put_u8(uint8_t** cursor, uint8_t value) {
    **cursor = value;
    *cursor += 1;
}

static uint8_t get_u8(const uint8_t** cursor) {
    const uint8_t value = **cursor;
    *cursor += 1;
    return value;
}

static void put_u16(uint8_t** cursor, uint16_t value) {
    (*cursor)[0] = (uint8_t)(value & 0xffu);
    (*cursor)[1] = (uint8_t)((value >> 8) & 0xffu);
    *cursor += 2;
}

static uint16_t get_u16(const uint8_t** cursor) {
    const uint16_t value = (uint16_t)((uint16_t)(*cursor)[0] |
                                      ((uint16_t)(*cursor)[1] << 8));
    *cursor += 2;
    return value;
}

static void put_u32(uint8_t** cursor, uint32_t value) {
    (*cursor)[0] = (uint8_t)(value & 0xffu);
    (*cursor)[1] = (uint8_t)((value >> 8) & 0xffu);
    (*cursor)[2] = (uint8_t)((value >> 16) & 0xffu);
    (*cursor)[3] = (uint8_t)((value >> 24) & 0xffu);
    *cursor += 4;
}

static uint32_t get_u32(const uint8_t** cursor) {
    const uint32_t value = (uint32_t)(*cursor)[0] |
                           ((uint32_t)(*cursor)[1] << 8) |
                           ((uint32_t)(*cursor)[2] << 16) |
                           ((uint32_t)(*cursor)[3] << 24);
    *cursor += 4;
    return value;
}

static void put_u64(uint8_t** cursor, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        (*cursor)[i] = (uint8_t)((value >> (8 * i)) & 0xffu);
    }
    *cursor += 8;
}

static uint64_t get_u64(const uint8_t** cursor) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= ((uint64_t)(*cursor)[i]) << (8 * i);
    }
    *cursor += 8;
    return value;
}

static void put_i32(uint8_t** cursor, int32_t value) {
    put_u32(cursor, (uint32_t)value);
}

static int32_t get_i32(const uint8_t** cursor) {
    return (int32_t)get_u32(cursor);
}

static void put_i64(uint8_t** cursor, int64_t value) {
    put_u64(cursor, (uint64_t)value);
}

static int64_t get_i64(const uint8_t** cursor) {
    return (int64_t)get_u64(cursor);
}

static void put_double(uint8_t** cursor, double value) {
    uint64_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    put_u64(cursor, raw);
}

static double get_double(const uint8_t** cursor) {
    const uint64_t raw = get_u64(cursor);
    double value = 0.0;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

uint32_t ff_tcp_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

void ff_tcp_encode_header(uint8_t* output, const FF_TcpHeader* header) {
    uint8_t* cursor = output;
    put_u32(&cursor, header->magic);
    put_u16(&cursor, header->version);
    put_u16(&cursor, header->msg_type);
    put_u32(&cursor, header->length);
    put_u32(&cursor, header->seq);
    put_u32(&cursor, header->crc32);
}

int ff_tcp_decode_header(const uint8_t* input, FF_TcpHeader* header) {
    const uint8_t* cursor = input;
    if (header == 0) {
        return FF_ERR;
    }
    header->magic = get_u32(&cursor);
    header->version = get_u16(&cursor);
    header->msg_type = get_u16(&cursor);
    header->length = get_u32(&cursor);
    header->seq = get_u32(&cursor);
    header->crc32 = get_u32(&cursor);
    return FF_OK;
}

int ff_tcp_validate_header(const FF_TcpHeader* header,
                           uint16_t expected_msg_type,
                           uint32_t expected_length,
                           const uint8_t* payload) {
    if (header == 0 || payload == 0) {
        return FF_ERR_PROTOCOL;
    }
    if (header->magic != FF_TCP_MAGIC ||
        header->version != FF_TCP_VERSION ||
        header->msg_type != expected_msg_type ||
        header->length != expected_length ||
        header->length > FF_TCP_MAX_PAYLOAD_SIZE) {
        return FF_ERR_PROTOCOL;
    }
    if (ff_tcp_crc32(payload, header->length) != header->crc32) {
        return FF_ERR_PROTOCOL;
    }
    return FF_OK;
}

void ff_tcp_encode_loco_cmd(uint8_t* output, const FF_LocoCmd* command) {
    uint8_t* cursor = output;
    put_double(&cursor, command->vx);
    put_double(&cursor, command->vy);
    put_double(&cursor, command->wz);
    put_u32(&cursor, command->duration_ms);
    put_u32(&cursor, command->flags);
}

int ff_tcp_decode_loco_cmd(const uint8_t* input, FF_LocoCmd* command) {
    const uint8_t* cursor = input;
    if (command == 0) return FF_ERR;
    memset(command, 0, sizeof(*command));
    command->vx = get_double(&cursor);
    command->vy = get_double(&cursor);
    command->wz = get_double(&cursor);
    command->duration_ms = get_u32(&cursor);
    command->flags = get_u32(&cursor);
    return FF_OK;
}

void ff_tcp_encode_mode_cmd(uint8_t* output, const FF_ModeCmd* command) {
    uint8_t* cursor = output;
    put_u8(&cursor, command->mode);
    put_u8(&cursor, 0);
    put_u8(&cursor, 0);
    put_u8(&cursor, 0);
}

int ff_tcp_decode_mode_cmd(const uint8_t* input, FF_ModeCmd* command) {
    const uint8_t* cursor = input;
    if (command == 0) return FF_ERR;
    memset(command, 0, sizeof(*command));
    command->mode = get_u8(&cursor);
    command->reserved[0] = get_u8(&cursor);
    command->reserved[1] = get_u8(&cursor);
    command->reserved[2] = get_u8(&cursor);
    if (command->mode > FF_LOCO_MODE_PRONE) return FF_ERR_PROTOCOL;
    return FF_OK;
}

void ff_tcp_encode_loco_state(uint8_t* output, const FF_LocoState* state) {
    uint8_t* cursor = output;
    put_i64(&cursor, state->timestamp_ns);
    put_u32(&cursor, state->acknowledged_seq);
    put_u8(&cursor, state->current_mode);
    put_u8(&cursor, state->hardware_rl_enabled);
    put_u8(&cursor, state->remote_command_active);
    put_u8(&cursor, state->client_connected);
    put_u8(&cursor, state->accepted);
    put_u8(&cursor, state->reject_reason);
    put_u8(&cursor, state->runtime_health);
    put_u8(&cursor, (uint8_t)state->fault_motor_id);
    put_u32(&cursor, state->remaining_ms);
    put_double(&cursor, state->applied_vx);
    put_double(&cursor, state->applied_vy);
    put_double(&cursor, state->applied_wz);
}

int ff_tcp_decode_loco_state(const uint8_t* input, FF_LocoState* state) {
    const uint8_t* cursor = input;
    if (state == 0) return FF_ERR;
    memset(state, 0, sizeof(*state));
    state->timestamp_ns = get_i64(&cursor);
    state->acknowledged_seq = get_u32(&cursor);
    state->current_mode = get_u8(&cursor);
    state->hardware_rl_enabled = get_u8(&cursor);
    state->remote_command_active = get_u8(&cursor);
    state->client_connected = get_u8(&cursor);
    state->accepted = get_u8(&cursor);
    state->reject_reason = get_u8(&cursor);
    state->runtime_health = get_u8(&cursor);
    state->fault_motor_id = (int8_t)get_u8(&cursor);
    state->remaining_ms = get_u32(&cursor);
    state->applied_vx = get_double(&cursor);
    state->applied_vy = get_double(&cursor);
    state->applied_wz = get_double(&cursor);
    return FF_OK;
}

void ff_tcp_encode_motor_state(uint8_t* output, const FF_MotorState* state) {
    uint8_t* cursor = output;
    put_i64(&cursor, state->sequence_id);
    put_i64(&cursor, state->timestamp_ns);
    put_u8(&cursor, state->state_valid);
    put_u8(&cursor, state->active_control_mode);
    put_u8(&cursor, state->command_fresh);
    put_u8(&cursor, state->damping_active);
    put_u8(&cursor, state->emergency_stop_latched);
    put_u8(&cursor, (uint8_t)state->safety_reason_code);
    put_u8(&cursor, 0);
    put_u8(&cursor, 0);
    put_i64(&cursor, state->command_age_ns);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) put_double(&cursor, state->q[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) put_double(&cursor, state->dq[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) put_double(&cursor, state->tau[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        put_double(&cursor, state->temperature_c[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        put_i32(&cursor, state->error_code[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        put_u8(&cursor, state->motor_valid[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        put_i64(&cursor, state->motor_feedback_timestamp_ns[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_double(&cursor, state->port_frequency_hz[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_success_count[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_failure_count[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_crc_error_count[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_timeout_count[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_no_reply_count[i]);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        put_i32(&cursor, state->foot_force_raw[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_invalid_feedback_count[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_last_success_age_ns[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_i64(&cursor, state->port_transaction_duration_ns[i]);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        put_u8(&cursor, state->port_transaction_ok[i]);
}

int ff_tcp_decode_motor_state(const uint8_t* input, FF_MotorState* state) {
    const uint8_t* cursor = input;
    if (state == 0) return FF_ERR;
    memset(state, 0, sizeof(*state));
    state->sequence_id = get_i64(&cursor);
    state->timestamp_ns = get_i64(&cursor);
    state->state_valid = get_u8(&cursor);
    state->active_control_mode = get_u8(&cursor);
    state->command_fresh = get_u8(&cursor);
    state->damping_active = get_u8(&cursor);
    state->emergency_stop_latched = get_u8(&cursor);
    state->safety_reason_code = (int8_t)get_u8(&cursor);
    state->reserved[0] = get_u8(&cursor);
    state->reserved[1] = get_u8(&cursor);
    state->command_age_ns = get_i64(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) state->q[i] = get_double(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) state->dq[i] = get_double(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) state->tau[i] = get_double(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        state->temperature_c[i] = get_double(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        state->error_code[i] = get_i32(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        state->motor_valid[i] = get_u8(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        state->motor_feedback_timestamp_ns[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_frequency_hz[i] = get_double(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_success_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_failure_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_crc_error_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_timeout_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_no_reply_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_MOTOR_COUNT; ++i)
        state->foot_force_raw[i] = get_i32(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_invalid_feedback_count[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_last_success_age_ns[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_transaction_duration_ns[i] = get_i64(&cursor);
    for (int i = 0; i < FF_PORT_COUNT; ++i)
        state->port_transaction_ok[i] = get_u8(&cursor);
    return FF_OK;
}
