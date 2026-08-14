#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FF_MOTOR_COUNT 12
#define FF_PORT_COUNT 4

#define FF_OK 0
#define FF_ERR -1
#define FF_ERR_TIMEOUT -2
#define FF_ERR_PROTOCOL -3
#define FF_ERR_DISCONNECTED -4

#define FF_LOCO_DEFAULT_PORT 9001
#define FF_LOCO_DEFAULT_DURATION_MS 2000u
#define FF_LOCO_MAX_DURATION_MS 60000u

typedef enum FF_LocoMode {
    FF_LOCO_MODE_ZERO_TORQUE = 0,
    FF_LOCO_MODE_STAND = 1,
    FF_LOCO_MODE_RL = 2,
    FF_LOCO_MODE_PRONE = 3
} FF_LocoMode;

typedef enum FF_LocoRejectReason {
    FF_LOCO_REJECT_NONE = 0,
    FF_LOCO_REJECT_BAD_COMMAND = 1,
    FF_LOCO_REJECT_PROTECTION_ACTIVE = 2,
    FF_LOCO_REJECT_RL_DISABLED = 3,
    FF_LOCO_REJECT_STAND_REQUIRED = 4,
    FF_LOCO_REJECT_STAND_NOT_READY = 5,
    FF_LOCO_REJECT_INTERNAL_MODE = 6,
    FF_LOCO_REJECT_RL_REQUIRED = 7,
    FF_LOCO_REJECT_LOCAL_OVERRIDE = 8,
    FF_LOCO_REJECT_CONTROLLER_TIMEOUT = 9,
    FF_LOCO_REJECT_RUNTIME_NOT_READY = 10
} FF_LocoRejectReason;

typedef enum FF_RuntimeHealth {
    FF_RUNTIME_INITIALIZING = 0,
    FF_RUNTIME_READY = 1,
    FF_RUNTIME_MOTOR_INIT_FAILED = 2,
    FF_RUNTIME_MOTOR_POSITION_INVALID = 3,
    FF_RUNTIME_FSM_INIT_FAILED = 4
} FF_RuntimeHealth;

// Read-only low-level control mode reported by the robot runtime. These values
// describe what the hardware service is actually applying; they are not a
// public motor-command API.
typedef enum FF_LowLevelControlMode {
    FF_LOW_LEVEL_CONTROL_DISABLED = 0,
    FF_LOW_LEVEL_CONTROL_DAMPING = 1,
    FF_LOW_LEVEL_CONTROL_POSITION_PD = 2,
    FF_LOW_LEVEL_CONTROL_HYBRID_RESERVED = 3
} FF_LowLevelControlMode;

typedef enum FF_SafetyReasonCode {
    FF_SAFETY_REASON_UNKNOWN = 0,
    FF_SAFETY_REASON_MOTOR_FEEDBACK_INVALID = 1,
    FF_SAFETY_REASON_EMERGENCY_STOP_LATCHED = 2,
    FF_SAFETY_REASON_COMMAND_WATCHDOG_EXPIRED = 3,
    FF_SAFETY_REASON_IMU_INVALID = 4,
    FF_SAFETY_REASON_REARM_REQUIRED = 5,
    FF_SAFETY_REASON_VALID_POSITION_PD = 6,
    FF_SAFETY_REASON_CONTROLLER_DISABLED = 7,
    FF_SAFETY_REASON_CONTROLLER_DAMPING = 8,
    FF_SAFETY_REASON_BACKEND_READ_FAILURE = 9,
    FF_SAFETY_REASON_COMMAND_ENQUEUE_FAILURE = 10,
    FF_SAFETY_REASON_SHUTDOWN_DAMPING = 11,
    FF_SAFETY_REASON_SHADOW_SAMPLE = 12
} FF_SafetyReasonCode;

typedef struct FF_LocoCmd {
    double vx;
    double vy;
    double wz;
    uint32_t duration_ms;
    uint32_t flags;
} FF_LocoCmd;

typedef struct FF_ModeCmd {
    uint8_t mode;
    uint8_t reserved[3];
} FF_ModeCmd;

typedef struct FF_LocoState {
    int64_t timestamp_ns;
    uint32_t acknowledged_seq;
    uint8_t current_mode;
    uint8_t hardware_rl_enabled;
    uint8_t remote_command_active;
    uint8_t client_connected;
    uint8_t accepted;
    uint8_t reject_reason;
    uint8_t runtime_health;
    int8_t fault_motor_id;
    uint32_t remaining_ms;
    double applied_vx;
    double applied_vy;
    double applied_wz;
} FF_LocoState;

// Read-only motor telemetry returned by the high-level SDK connection.
// Joint arrays keep the runtime hardware motor-ID order [0..11].
typedef struct FF_MotorState {
    int64_t sequence_id;
    int64_t timestamp_ns;
    uint8_t state_valid;

    // Read-only control/safety metadata from the LCM hardware service.
    uint8_t active_control_mode;
    uint8_t command_fresh;
    uint8_t damping_active;
    uint8_t emergency_stop_latched;
    int8_t safety_reason_code;
    uint8_t reserved[2];
    int64_t command_age_ns;

    double q[FF_MOTOR_COUNT];
    double dq[FF_MOTOR_COUNT];
    double tau[FF_MOTOR_COUNT];
    double temperature_c[FF_MOTOR_COUNT];
    int32_t error_code[FF_MOTOR_COUNT];
    uint8_t motor_valid[FF_MOTOR_COUNT];
    int64_t motor_feedback_timestamp_ns[FF_MOTOR_COUNT];

    double port_frequency_hz[FF_PORT_COUNT];
    int64_t port_success_count[FF_PORT_COUNT];
    int64_t port_failure_count[FF_PORT_COUNT];
    int64_t port_crc_error_count[FF_PORT_COUNT];
    int64_t port_timeout_count[FF_PORT_COUNT];
    int64_t port_no_reply_count[FF_PORT_COUNT];

    // Uncalibrated raw foot-force feedback (12-bit field, 0-4095). This is the
    // raw sensor field from motor feedback, not a physical foot force.
    int32_t foot_force_raw[FF_MOTOR_COUNT];

    // Per-port communication freshness/quality. The existing CRC/timeout/
    // no-reply counters above stay -1 because the installed SDK only reports a
    // combined transaction failure.
    int64_t port_invalid_feedback_count[FF_PORT_COUNT];
    int64_t port_last_success_age_ns[FF_PORT_COUNT];
    int64_t port_transaction_duration_ns[FF_PORT_COUNT];
    uint8_t port_transaction_ok[FF_PORT_COUNT];
} FF_MotorState;

int ff_loco_connect(const char* ip, int port);
int ff_loco_move(double vx, double vy, double wz, uint32_t duration_ms);
int ff_loco_stop(void);
int ff_loco_set_mode(FF_LocoMode mode);
int ff_loco_recv_state(FF_LocoState* state, int timeout_ms);
int ff_loco_recv_motor_state(FF_MotorState* state, int timeout_ms);
void ff_loco_close(void);
const char* ff_loco_strerror(int code);
const char* ff_loco_reject_reason_string(uint8_t reason);
const char* ff_loco_runtime_health_string(uint8_t health);
const char* ff_loco_control_mode_string(uint8_t mode);
const char* ff_loco_safety_reason_string(int8_t reason);

#ifdef __cplusplus
}
#endif
