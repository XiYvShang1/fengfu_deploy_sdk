#include "fengfu_loco.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    const char* ip = argc > 1 ? argv[1] : "192.168.0.125";
    const int port = argc > 2 ? std::atoi(argv[2]) : FF_LOCO_DEFAULT_PORT;
    try {
        fengfu::FengfuRobot robot;
        robot.connect(ip, port);
        std::cout << "connected; requesting STAND\n";
        robot.stand();
        std::this_thread::sleep_for(std::chrono::seconds(4));
        std::cout << "requesting RL\n";
        robot.enterLoco();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "move forward at vx=0.30 m/s for 2 seconds\n";
        robot.move(0.30, 0.0, 0.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        robot.stop();
        std::cout << "velocity stopped; robot remains in RL\n";
    } catch (const std::exception& error) {
        std::cerr << "loco_move_demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
