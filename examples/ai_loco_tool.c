#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 199309L
#endif

#include "fengfu_loco_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_ms(uint32_t duration_ms) { Sleep(duration_ms); }
#else
#include <time.h>
static void sleep_ms(uint32_t duration_ms) {
    struct timespec delay;
    delay.tv_sec = (time_t)(duration_ms / 1000u);
    delay.tv_nsec = (long)(duration_ms % 1000u) * 1000000L;
    while (nanosleep(&delay, &delay) != 0) {
    }
}
#endif

static void usage(const char* program) {
    fprintf(stderr,
            "用法：\n"
            "  %s <ip> move <vx> <vy> <wz> [duration_ms]\n"
            "  %s <ip> stop|stand|rl|prone|zero|state|motor|console\n",
            program, program);
}

static const char* health_en(uint8_t health) {
    switch (health) {
        case FF_RUNTIME_INITIALIZING: return "initializing";
        case FF_RUNTIME_READY: return "ready";
        case FF_RUNTIME_MOTOR_INIT_FAILED: return "motor_init_failed";
        case FF_RUNTIME_MOTOR_POSITION_INVALID: return "motor_position_invalid";
        case FF_RUNTIME_FSM_INIT_FAILED: return "fsm_init_failed";
        default: return "unknown";
    }
}

static const char* mode_en(uint8_t mode) {
    switch (mode) {
        case FF_LOCO_MODE_ZERO_TORQUE: return "zero_torque";
        case FF_LOCO_MODE_STAND: return "stand";
        case FF_LOCO_MODE_RL: return "rl";
        case FF_LOCO_MODE_PRONE: return "prone";
        default: return "unknown";
    }
}

static const char* motor_name_en(int id) {
    static const char* names[FF_MOTOR_COUNT] = {
        "FR_hip", "FR_thigh", "FR_calf",
        "FL_hip", "FL_thigh", "FL_calf",
        "RR_hip", "RR_thigh", "RR_calf",
        "RL_hip", "RL_thigh", "RL_calf"
    };
    return id >= 0 && id < FF_MOTOR_COUNT ? names[id] : "unknown";
}

static const char* error_en(int error) {
    switch (error) {
        case 0: return "OK";
        case 1: return "OVERHEAT";
        case 2: return "OVERCURRENT";
        case 3: return "OVERVOLTAGE";
        case 4: return "ENCODER_FAULT";
        default: return "UNKNOWN";
    }
}

static void print_age_ns(const char* name, int64_t age_ns) {
    if (age_ns < 0) {
        printf("%s=never ", name);
    } else if (age_ns < 1000000) {
        printf("%s=%lldus_ago ", name, (long long)(age_ns / 1000));
    } else {
        printf("%s=%.1fms_ago ", name, (double)age_ns / 1.0e6);
    }
}

static void print_counter(const char* name, int64_t value) {
    if (value < 0) {
        printf("%s=unsupported ", name);
    } else {
        printf("%s=%lld ", name, (long long)value);
    }
}

static int64_t timestamp_age_ns(int64_t packet_timestamp_ns,
                                int64_t sample_timestamp_ns) {
    if (packet_timestamp_ns <= 0 || sample_timestamp_ns <= 0 ||
        sample_timestamp_ns > packet_timestamp_ns) {
        return -1;
    }
    return packet_timestamp_ns - sample_timestamp_ns;
}

static void print_uptime_ns(const char* name, int64_t timestamp_ns) {
    if (timestamp_ns < 0) {
        printf("%s=unknown ", name);
        return;
    }
    const int64_t total_ms = timestamp_ns / 1000000;
    const int64_t hours = total_ms / 3600000;
    const int64_t minutes = (total_ms / 60000) % 60;
    const double seconds = (double)(total_ms % 60000) / 1000.0;
    printf("%s=%lldh%02lldm%06.3fs ", name, (long long)hours,
           (long long)minutes, seconds);
}

static int print_state(void) {
    FF_LocoState state;
    const int result = ff_loco_recv_state(&state, 1000);
    if (result != FF_OK) {
        fprintf(stderr, "获取状态失败：%s\n", ff_loco_strerror(result));
        return result;
    }
    printf("timestamp_ns=%lld ", (long long)state.timestamp_ns);
    print_uptime_ns("server_uptime", state.timestamp_ns);
    printf("ack_seq=%u client_connected=%u\n", state.acknowledged_seq,
           state.client_connected);
    printf("health=%s motor_init=%s mode=%s rl_enabled=%u "
           "remote_active=%s remaining_ms=%u velocity=[%.3f %.3f %.3f] "
           "last_cmd=%s reject_reason=%s fault_motor_id=%d\n",
           health_en(state.runtime_health),
           state.runtime_health == FF_RUNTIME_READY ? "passed_12/12" : "not_passed",
           mode_en(state.current_mode), state.hardware_rl_enabled,
           state.remote_command_active ? "active" : "idle",
           state.remaining_ms,
           state.applied_vx, state.applied_vy, state.applied_wz,
           state.accepted ? "accepted" : "rejected",
           ff_loco_reject_reason_string(state.reject_reason),
           (int)state.fault_motor_id);
    if (state.runtime_health == FF_RUNTIME_INITIALIZING) {
        fprintf(stderr,
                "\n【初始化中】机器狗正在完成电机、IMU和FSM初始化。\n"
                "当前暂不接受PRONE、STAND、RL及速度命令，这是正常现象，无需重启。\n"
                "请稍候再输入 state；显示 health=ready 后即可开始操作。\n\n");
    } else if (state.runtime_health != FF_RUNTIME_READY) {
        fprintf(stderr,
                "\n【安全警报】%s。\n"
                "检测位置：%s（内部编号 M%d）。\n"
                "运动控制已锁定，PRONE、STAND、RL及速度命令均不会执行。\n"
                "请检查机器狗周围安全后，完整重启机器狗，再重新连接检查。\n\n",
                health_en(state.runtime_health),
                motor_name_en((int)state.fault_motor_id),
                (int)state.fault_motor_id);
    }
    return FF_OK;
}

static int print_motor_state(void) {
    FF_MotorState state;
    const int result = ff_loco_recv_motor_state(&state, 1000);
    if (result != FF_OK) {
        fprintf(stderr, "获取电机状态失败：%s\n", ff_loco_strerror(result));
        return result;
    }
    printf("state_valid=%u sequence=%lld timestamp_ns=%lld ",
           state.state_valid, (long long)state.sequence_id,
           (long long)state.timestamp_ns);
    print_uptime_ns("server_uptime", state.timestamp_ns);
    printf("\n");
    printf("control_mode=%s command_fresh=%u ",
           ff_loco_control_mode_string(state.active_control_mode),
           state.command_fresh);
    print_age_ns("command_age", state.command_age_ns);
    printf("damping_active=%u emergency_stop_latched=%u safety_reason=%s\n",
           state.damping_active, state.emergency_stop_latched,
           ff_loco_safety_reason_string(state.safety_reason_code));
    for (int i = 0; i < FF_MOTOR_COUNT; ++i) {
        printf("M%02d %-8s q=%+.4f dq=%+.4f tau=%+.3f temp=%.0fC err=%s "
               "valid=%u force=%d feedback_timestamp_ns=%lld ",
               i, motor_name_en(i), state.q[i], state.dq[i], state.tau[i],
               state.temperature_c[i], error_en(state.error_code[i]),
               state.motor_valid[i], state.foot_force_raw[i],
               (long long)state.motor_feedback_timestamp_ns[i]);
        print_uptime_ns("feedback_uptime",
                        state.motor_feedback_timestamp_ns[i]);
        print_age_ns("feedback_age",
                     timestamp_age_ns(state.timestamp_ns,
                                      state.motor_feedback_timestamp_ns[i]));
        printf("\n");
    }
    printf("\nport_diagnostics (3 motors per port):\n");
    for (int port = 0; port < FF_PORT_COUNT; ++port) {
        printf("port%d freq=%.1fHz success=%lld failure=%lld invalid_fb=%lld\n",
               port, state.port_frequency_hz[port],
               (long long)state.port_success_count[port],
               (long long)state.port_failure_count[port],
               (long long)state.port_invalid_feedback_count[port]);
        printf("       ");
        print_age_ns("last_success", state.port_last_success_age_ns[port]);
        printf("tx_duration=%.0fus tx_ok=%u ",
               (double)state.port_transaction_duration_ns[port] / 1000.0,
               state.port_transaction_ok[port] ? 1u : 0u);
        print_counter("crc", state.port_crc_error_count[port]);
        print_counter("timeout", state.port_timeout_count[port]);
        print_counter("no_reply", state.port_no_reply_count[port]);
        printf("\n");
    }
    return FF_OK;
}

static void print_console_help(void) {
    puts("\n推荐操作流程：");
    puts("  1. prone                 进入趴下姿态，先确认机器狗响应正常");
    puts("  2. stand                 从趴下状态站立，确认站稳");
    puts("  3. rl                    进入RL行走模式");
    puts("  4. move vx vy wz [ms]    发送前后/平移/旋转速度和持续毫秒数");
    puts("  5. stand 或 prone        退出RL，回到站立或趴下状态");
    puts("  6. zero                  测试结束，释放为零力矩\n");
    puts("命令说明：");
    puts("  state                 查看运行状态和初始化结果");
    puts("  motor                 查看12个电机只读状态");
    puts("  stand / prone / zero  站立 / 趴下 / 零力矩");
    puts("  rl                    从站立状态进入RL行走");
    puts("  move vx vy wz [ms]    前后/平移/旋转速度及持续毫秒数");
    puts("  stop                  立即把速度指令置零");
    puts("  help                  再次显示操作流程");
    puts("  quit                  退出控制台（退出前建议先输入 zero）\n");
}

static int run_console(const char* ip) {
    char line[256];
    puts("\n=== Fengfu 机器狗二次开发控制台 ===");
    printf("已连接机器狗 %s:%d，进入高层控制系统。\n",
           ip, FF_LOCO_DEFAULT_PORT);
    puts("状态数据保留英文 key=value 格式，便于程序和AI直接解析。");
    print_console_help();
    puts("当前运行状态：");
    print_state();
    for (;;) {
        double vx = 0.0, vy = 0.0, wz = 0.0;
        unsigned int duration_ms = FF_LOCO_DEFAULT_DURATION_MS;
        int result = FF_OK;
        const char* success_hint = NULL;
        printf("fengfu> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
        if (strcmp(line, "state") == 0) {
            print_state();
            continue;
        } else if (strcmp(line, "motor") == 0) {
            print_motor_state();
            continue;
        } else if (strcmp(line, "help") == 0) {
            print_console_help();
            continue;
        } else if (strcmp(line, "stand") == 0) {
            result = ff_loco_set_mode(FF_LOCO_MODE_STAND);
            success_hint = "已请求站立。确认机器狗完全站稳后，再输入 rl。";
        } else if (strcmp(line, "rl") == 0) {
            result = ff_loco_set_mode(FF_LOCO_MODE_RL);
            success_hint =
                "已请求进入RL。可输入 move；输入 stand 或 prone 可退出RL。";
        } else if (strcmp(line, "stop") == 0) {
            result = ff_loco_stop();
            success_hint = "速度指令已归零，机器狗仍保持当前FSM模式。";
        } else if (strcmp(line, "prone") == 0) {
            result = ff_loco_set_mode(FF_LOCO_MODE_PRONE);
            success_hint =
                "已请求趴下。确认姿态和电机响应正常后，可输入 stand。";
        } else if (strcmp(line, "zero") == 0) {
            result = ff_loco_set_mode(FF_LOCO_MODE_ZERO_TORQUE);
            success_hint = "已请求零力矩，测试流程结束后可以输入 quit。";
        } else if (sscanf(line, "move %lf %lf %lf %u",
                          &vx, &vy, &wz, &duration_ms) >= 3) {
            result = ff_loco_move(vx, vy, wz, duration_ms);
        } else {
            puts("未知命令。请输入 state、motor、stand、rl、move、stop、prone、zero 或 quit。");
            continue;
        }
        if (result != FF_OK) {
            fprintf(stderr, "命令被拒绝：%s。请先输入 state 查看原因。\n",
                    ff_loco_strerror(result));
        } else if (success_hint != NULL) {
            puts(success_hint);
            if (strncmp(line, "move ", 5) == 0)
                printf("速度命令持续 %u ms；需要提前停止时输入 stop。\n",
                       duration_ms);
        }
        print_state();
    }
    ff_loco_stop();
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    int result = ff_loco_connect(argv[1], FF_LOCO_DEFAULT_PORT);
    if (result != FF_OK) {
        fprintf(stderr, "连接机器狗失败：%s\n", ff_loco_strerror(result));
        return 1;
    }

    const char* action = argv[2];
    if (strcmp(action, "console") == 0) {
        const int console_result = run_console(argv[1]);
        ff_loco_close();
        return console_result;
    }
    uint32_t move_duration = 0;
    if (strcmp(action, "move") == 0 && argc >= 6) {
        move_duration = argc >= 7
                            ? (uint32_t)strtoul(argv[6], 0, 10)
                            : FF_LOCO_DEFAULT_DURATION_MS;
        result = ff_loco_move(strtod(argv[3], 0), strtod(argv[4], 0),
                              strtod(argv[5], 0), move_duration);
    } else if (strcmp(action, "stop") == 0) {
        result = ff_loco_stop();
    } else if (strcmp(action, "stand") == 0) {
        result = ff_loco_set_mode(FF_LOCO_MODE_STAND);
    } else if (strcmp(action, "rl") == 0) {
        result = ff_loco_set_mode(FF_LOCO_MODE_RL);
    } else if (strcmp(action, "prone") == 0) {
        result = ff_loco_set_mode(FF_LOCO_MODE_PRONE);
    } else if (strcmp(action, "zero") == 0) {
        result = ff_loco_set_mode(FF_LOCO_MODE_ZERO_TORQUE);
    } else if (strcmp(action, "motor") == 0) {
        result = print_motor_state();
    } else if (strcmp(action, "state") != 0) {
        usage(argv[0]);
        ff_loco_close();
        return 2;
    }

    if (result == FF_OK && move_duration > 0) {
        /* Keep the safety session alive until the timed command expires. */
        sleep_ms(move_duration + 20u);
    }

    const int state_result = print_state();
    ff_loco_close();
    if (result != FF_OK) {
        fprintf(stderr, "%s: %s\n", action, ff_loco_strerror(result));
        return 1;
    }
    return state_result == FF_OK ? 0 : 1;
}
