#pragma once

#include "fengfu_loco_sdk.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FF_TCP_MAGIC 0x46534655u
#define FF_TCP_VERSION 1u
#define FF_TCP_HEADER_SIZE 20u
#define FF_TCP_MAX_PAYLOAD_SIZE 4096u
#define FF_TCP_LOCO_CMD_PAYLOAD_SIZE 32u
#define FF_TCP_MODE_CMD_PAYLOAD_SIZE 4u
#define FF_TCP_LOCO_STATE_PAYLOAD_SIZE 48u
#define FF_TCP_MOTOR_STATE_PAYLOAD_SIZE 912u

typedef enum FF_TcpMessageType {
    FF_TCP_MSG_HEARTBEAT = 3,
    FF_TCP_MSG_LOCO_CMD = 10,
    FF_TCP_MSG_MODE_CMD = 11,
    FF_TCP_MSG_LOCO_STATE = 12,
    FF_TCP_MSG_MOTOR_STATE_REQUEST = 13,
    FF_TCP_MSG_MOTOR_STATE = 14
} FF_TcpMessageType;

typedef struct FF_TcpHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t msg_type;
    uint32_t length;
    uint32_t seq;
    uint32_t crc32;
} FF_TcpHeader;

uint32_t ff_tcp_crc32(const uint8_t* data, size_t length);
void ff_tcp_encode_header(uint8_t* output, const FF_TcpHeader* header);
int ff_tcp_decode_header(const uint8_t* input, FF_TcpHeader* header);
int ff_tcp_validate_header(const FF_TcpHeader* header,
                           uint16_t expected_msg_type,
                           uint32_t expected_length,
                           const uint8_t* payload);
void ff_tcp_encode_loco_cmd(uint8_t* output, const FF_LocoCmd* command);
int ff_tcp_decode_loco_cmd(const uint8_t* input, FF_LocoCmd* command);
void ff_tcp_encode_mode_cmd(uint8_t* output, const FF_ModeCmd* command);
int ff_tcp_decode_mode_cmd(const uint8_t* input, FF_ModeCmd* command);
void ff_tcp_encode_loco_state(uint8_t* output, const FF_LocoState* state);
int ff_tcp_decode_loco_state(const uint8_t* input, FF_LocoState* state);
void ff_tcp_encode_motor_state(uint8_t* output, const FF_MotorState* state);
int ff_tcp_decode_motor_state(const uint8_t* input, FF_MotorState* state);

#ifdef __cplusplus
}
#endif
