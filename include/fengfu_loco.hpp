#pragma once

#include "fengfu_loco_sdk.h"

#include <stdexcept>
#include <string>

namespace fengfu {

class FengfuRobot {
public:
    FengfuRobot() = default;
    ~FengfuRobot() { close(); }
    FengfuRobot(const FengfuRobot&) = delete;
    FengfuRobot& operator=(const FengfuRobot&) = delete;

    void connect(const std::string& ip, int port = FF_LOCO_DEFAULT_PORT) {
        check(ff_loco_connect(ip.c_str(), port), "connect");
        connected_ = true;
    }

    void stand() { check(ff_loco_set_mode(FF_LOCO_MODE_STAND), "stand"); }
    void enterLoco() { check(ff_loco_set_mode(FF_LOCO_MODE_RL), "enterLoco"); }
    void prone() { check(ff_loco_set_mode(FF_LOCO_MODE_PRONE), "prone"); }
    void zeroTorque() { check(ff_loco_set_mode(FF_LOCO_MODE_ZERO_TORQUE), "zeroTorque"); }

    void move(double vx, double vy, double wz,
              uint32_t duration_ms = FF_LOCO_DEFAULT_DURATION_MS) {
        check(ff_loco_move(vx, vy, wz, duration_ms), "move");
    }

    void stop() { check(ff_loco_stop(), "stop"); }

    FF_LocoState receiveState(int timeout_ms = 1000) {
        FF_LocoState state{};
        check(ff_loco_recv_state(&state, timeout_ms), "receiveState");
        return state;
    }

    FF_MotorState motorState(int timeout_ms = 1000) {
        FF_MotorState state{};
        check(ff_loco_recv_motor_state(&state, timeout_ms), "motorState");
        return state;
    }

    void close() noexcept {
        if (connected_) {
            ff_loco_close();
            connected_ = false;
        }
    }

private:
    static void check(int result, const char* action) {
        if (result != FF_OK) {
            throw std::runtime_error(std::string(action) + ": " +
                                     ff_loco_strerror(result));
        }
    }

    bool connected_ = false;
};

}  // namespace fengfu
