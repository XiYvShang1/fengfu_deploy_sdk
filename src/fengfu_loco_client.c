#include "fengfu_loco_sdk.h"
#include "fengfu_loco_protocol.h"

#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ff_loco_socket_t;
#define FF_LOCO_INVALID_SOCKET INVALID_SOCKET
#define ff_loco_close_socket closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int ff_loco_socket_t;
#define FF_LOCO_INVALID_SOCKET (-1)
#define ff_loco_close_socket close
#endif

static ff_loco_socket_t g_loco_socket = FF_LOCO_INVALID_SOCKET;
static uint32_t g_loco_sequence = 0;

static int send_frame(uint16_t type, const uint8_t* payload, uint32_t size,
                      uint32_t* sequence);

static int loco_socket_startup(void) {
#if defined(_WIN32)
    static int initialized = 0;
    if (!initialized) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return FF_ERR;
        initialized = 1;
    }
#endif
    return FF_OK;
}

static int wait_readable(ff_loco_socket_t fd, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    struct timeval timeout;
    struct timeval* timeout_ptr = 0;
    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
    }
    const int result = select((int)(fd + 1), &readfds, 0, 0, timeout_ptr);
    if (result == 0) return FF_ERR_TIMEOUT;
    if (result < 0) return FF_ERR;
    return FF_OK;
}

static int recv_exact(ff_loco_socket_t fd, uint8_t* data, size_t size,
                      int timeout_ms) {
    size_t offset = 0;
    while (offset < size) {
        const int ready = wait_readable(fd, timeout_ms);
        if (ready != FF_OK) return ready;
        const int count = (int)recv(fd, (char*)data + offset,
                                    (int)(size - offset), 0);
        if (count == 0) return FF_ERR_DISCONNECTED;
        if (count < 0) return FF_ERR;
        offset += (size_t)count;
    }
    return FF_OK;
}

static int send_exact(ff_loco_socket_t fd, const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
#if defined(MSG_NOSIGNAL)
        const int send_flags = MSG_NOSIGNAL;
#else
        const int send_flags = 0;
#endif
        const int count = (int)send(fd, (const char*)data + offset,
                                    (int)(size - offset), send_flags);
        if (count <= 0) return FF_ERR_DISCONNECTED;
        offset += (size_t)count;
    }
    return FF_OK;
}

static int send_frame(uint16_t type, const uint8_t* payload, uint32_t size,
                      uint32_t* sequence) {
    if (g_loco_socket == FF_LOCO_INVALID_SOCKET) return FF_ERR_DISCONNECTED;
    uint8_t frame[FF_TCP_HEADER_SIZE + FF_TCP_MAX_PAYLOAD_SIZE];
    FF_TcpHeader header;
    header.magic = FF_TCP_MAGIC;
    header.version = FF_TCP_VERSION;
    header.msg_type = type;
    header.length = size;
    header.seq = ++g_loco_sequence;
    header.crc32 = ff_tcp_crc32(payload, size);
    ff_tcp_encode_header(frame, &header);
    memcpy(frame + FF_TCP_HEADER_SIZE, payload, size);
    const int result = send_exact(g_loco_socket, frame, FF_TCP_HEADER_SIZE + size);
    if (result == FF_OK && sequence != 0) *sequence = header.seq;
    return result;
}

static int recv_state_raw(FF_LocoState* state, int timeout_ms) {
    uint8_t header_bytes[FF_TCP_HEADER_SIZE];
    uint8_t payload[FF_TCP_MAX_PAYLOAD_SIZE];
    FF_TcpHeader header;
    int result;
    result = recv_exact(g_loco_socket, header_bytes, sizeof(header_bytes), timeout_ms);
    if (result != FF_OK) return result;
    if (ff_tcp_decode_header(header_bytes, &header) != FF_OK ||
        header.length > FF_TCP_MAX_PAYLOAD_SIZE) return FF_ERR_PROTOCOL;
    result = recv_exact(g_loco_socket, payload, header.length, timeout_ms);
    if (result != FF_OK) return result;
    result = ff_tcp_validate_header(&header, FF_TCP_MSG_LOCO_STATE,
                                    FF_TCP_LOCO_STATE_PAYLOAD_SIZE, payload);
    if (result != FF_OK) return result;
    return ff_tcp_decode_loco_state(payload, state);
}

static int recv_motor_state_raw(FF_MotorState* state, int timeout_ms) {
    uint8_t header_bytes[FF_TCP_HEADER_SIZE];
    uint8_t payload[FF_TCP_MAX_PAYLOAD_SIZE];
    FF_TcpHeader header;
    int result = recv_exact(g_loco_socket, header_bytes, sizeof(header_bytes),
                            timeout_ms);
    if (result != FF_OK) return result;
    if (ff_tcp_decode_header(header_bytes, &header) != FF_OK ||
        header.length > FF_TCP_MAX_PAYLOAD_SIZE) return FF_ERR_PROTOCOL;
    result = recv_exact(g_loco_socket, payload, header.length, timeout_ms);
    if (result != FF_OK) return result;
    result = ff_tcp_validate_header(&header, FF_TCP_MSG_MOTOR_STATE,
                                    FF_TCP_MOTOR_STATE_PAYLOAD_SIZE, payload);
    if (result != FF_OK) return result;
    return ff_tcp_decode_motor_state(payload, state);
}

static int recv_state_for_sequence(uint32_t expected_sequence,
                                   FF_LocoState* output,
                                   int timeout_ms) {
    for (;;) {
        FF_LocoState state;
        const int result = recv_state_raw(&state, timeout_ms);
        if (result != FF_OK) return result;
        if (state.acknowledged_seq < expected_sequence) continue;
        if (output != 0) *output = state;
        return state.accepted ? FF_OK : FF_ERR_PROTOCOL;
    }
}

static int request_state(void) {
    static const uint8_t empty_payload[1] = {0};
    uint32_t ignored_sequence = 0;
    return send_frame(FF_TCP_MSG_HEARTBEAT, empty_payload, 0,
                      &ignored_sequence);
}

int ff_loco_connect(const char* ip, int port) {
    if (ip == 0 || port <= 0 || port > 65535) return FF_ERR;
    ff_loco_close();
    if (loco_socket_startup() != FF_OK) return FF_ERR;
    ff_loco_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == FF_LOCO_INVALID_SOCKET) return FF_ERR;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1 ||
        connect(fd, (struct sockaddr*)&address, sizeof(address)) != 0) {
        ff_loco_close_socket(fd);
        return FF_ERR;
    }
    g_loco_socket = fd;
    g_loco_sequence = 0;
    return FF_OK;
}

int ff_loco_move(double vx, double vy, double wz, uint32_t duration_ms) {
    FF_LocoCmd command;
    uint8_t payload[FF_TCP_LOCO_CMD_PAYLOAD_SIZE];
    memset(&command, 0, sizeof(command));
    command.vx = vx;
    command.vy = vy;
    command.wz = wz;
    command.duration_ms = duration_ms == 0 ? FF_LOCO_DEFAULT_DURATION_MS : duration_ms;
    ff_tcp_encode_loco_cmd(payload, &command);
    uint32_t sequence = 0;
    const int result = send_frame(FF_TCP_MSG_LOCO_CMD, payload,
                                  sizeof(payload), &sequence);
    return result == FF_OK
               ? recv_state_for_sequence(sequence, 0, 1000)
               : result;
}

int ff_loco_stop(void) {
    FF_LocoCmd command;
    uint8_t payload[FF_TCP_LOCO_CMD_PAYLOAD_SIZE];
    memset(&command, 0, sizeof(command));
    command.duration_ms = 1;
    ff_tcp_encode_loco_cmd(payload, &command);
    uint32_t sequence = 0;
    const int result = send_frame(FF_TCP_MSG_LOCO_CMD, payload,
                                  sizeof(payload), &sequence);
    return result == FF_OK
               ? recv_state_for_sequence(sequence, 0, 1000)
               : result;
}

int ff_loco_set_mode(FF_LocoMode mode) {
    if (mode < FF_LOCO_MODE_ZERO_TORQUE || mode > FF_LOCO_MODE_PRONE)
        return FF_ERR_PROTOCOL;
    FF_ModeCmd command;
    uint8_t payload[FF_TCP_MODE_CMD_PAYLOAD_SIZE];
    memset(&command, 0, sizeof(command));
    command.mode = (uint8_t)mode;
    ff_tcp_encode_mode_cmd(payload, &command);
    uint32_t sequence = 0;
    const int result = send_frame(FF_TCP_MSG_MODE_CMD, payload,
                                  sizeof(payload), &sequence);
    return result == FF_OK
               ? recv_state_for_sequence(sequence, 0, 2000)
               : result;
}

int ff_loco_recv_state(FF_LocoState* state, int timeout_ms) {
    int result;
    if (g_loco_socket == FF_LOCO_INVALID_SOCKET || state == 0) return FF_ERR;
    result = request_state();
    if (result != FF_OK) return result;
    return recv_state_raw(state, timeout_ms);
}

int ff_loco_recv_motor_state(FF_MotorState* state, int timeout_ms) {
    static const uint8_t empty_payload[1] = {0};
    uint32_t ignored_sequence = 0;
    int result;
    if (g_loco_socket == FF_LOCO_INVALID_SOCKET || state == 0) return FF_ERR;
    result = send_frame(FF_TCP_MSG_MOTOR_STATE_REQUEST, empty_payload, 0,
                        &ignored_sequence);
    if (result != FF_OK) return result;
    return recv_motor_state_raw(state, timeout_ms);
}

void ff_loco_close(void) {
    if (g_loco_socket != FF_LOCO_INVALID_SOCKET) {
        ff_loco_close_socket(g_loco_socket);
        g_loco_socket = FF_LOCO_INVALID_SOCKET;
    }
}

const char* ff_loco_strerror(int code) {
    switch (code) {
        case FF_OK:
            return "ok";
        case FF_ERR_TIMEOUT:
            return "timeout";
        case FF_ERR_PROTOCOL:
            return "protocol error";
        case FF_ERR_DISCONNECTED:
            return "disconnected";
        case FF_ERR:
        default:
            return "error";
    }
}

const char* ff_loco_reject_reason_string(uint8_t reason) {
    switch (reason) {
        case FF_LOCO_REJECT_NONE: return "accepted";
        case FF_LOCO_REJECT_BAD_COMMAND: return "bad command";
        case FF_LOCO_REJECT_PROTECTION_ACTIVE: return "protection active";
        case FF_LOCO_REJECT_RL_DISABLED: return "RL disabled";
        case FF_LOCO_REJECT_STAND_REQUIRED: return "STAND required";
        case FF_LOCO_REJECT_STAND_NOT_READY: return "STAND not ready";
        case FF_LOCO_REJECT_INTERNAL_MODE: return "internal mode forbidden";
        case FF_LOCO_REJECT_RL_REQUIRED: return "RL mode required";
        case FF_LOCO_REJECT_LOCAL_OVERRIDE: return "local operator override";
        case FF_LOCO_REJECT_CONTROLLER_TIMEOUT: return "controller timeout";
        case FF_LOCO_REJECT_RUNTIME_NOT_READY: return "runtime not ready";
        default: return "unknown rejection";
    }
}

const char* ff_loco_runtime_health_string(uint8_t health) {
    switch (health) {
        case FF_RUNTIME_INITIALIZING: return "initializing";
        case FF_RUNTIME_READY: return "ready";
        case FF_RUNTIME_MOTOR_INIT_FAILED:
            return "motor initialization failed; restart the robot";
        case FF_RUNTIME_MOTOR_POSITION_INVALID:
            return "motor position initialization invalid; restart the robot";
        case FF_RUNTIME_FSM_INIT_FAILED:
            return "FSM initialization failed; restart the robot";
        default: return "unknown runtime health";
    }
}

const char* ff_loco_control_mode_string(uint8_t mode) {
    switch (mode) {
        case FF_LOW_LEVEL_CONTROL_DISABLED: return "disabled";
        case FF_LOW_LEVEL_CONTROL_DAMPING: return "damping";
        case FF_LOW_LEVEL_CONTROL_POSITION_PD: return "position_pd";
        case FF_LOW_LEVEL_CONTROL_HYBRID_RESERVED: return "hybrid_reserved";
        default: return "unknown";
    }
}

const char* ff_loco_safety_reason_string(int8_t reason) {
    switch (reason) {
        case FF_SAFETY_REASON_UNKNOWN: return "unknown";
        case FF_SAFETY_REASON_MOTOR_FEEDBACK_INVALID:
            return "motor_feedback_invalid";
        case FF_SAFETY_REASON_EMERGENCY_STOP_LATCHED:
            return "emergency_stop_latched";
        case FF_SAFETY_REASON_COMMAND_WATCHDOG_EXPIRED:
            return "command_watchdog_expired";
        case FF_SAFETY_REASON_IMU_INVALID: return "imu_invalid";
        case FF_SAFETY_REASON_REARM_REQUIRED: return "rearm_required";
        case FF_SAFETY_REASON_VALID_POSITION_PD: return "valid_position_pd";
        case FF_SAFETY_REASON_CONTROLLER_DISABLED:
            return "controller_disabled";
        case FF_SAFETY_REASON_CONTROLLER_DAMPING:
            return "controller_damping";
        case FF_SAFETY_REASON_BACKEND_READ_FAILURE:
            return "backend_read_failure";
        case FF_SAFETY_REASON_COMMAND_ENQUEUE_FAILURE:
            return "command_enqueue_failure";
        case FF_SAFETY_REASON_SHUTDOWN_DAMPING:
            return "shutdown_damping";
        case FF_SAFETY_REASON_SHADOW_SAMPLE: return "shadow_sample";
        default: return "unknown";
    }
}
