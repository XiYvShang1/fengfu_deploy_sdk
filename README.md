# Fengfu 高层运动客户端 SDK

本目录是可独立交付的高层客户端工程，只连接机器狗 TCP `9001` 端口。

对外提供：

- 高层模式控制：趴下、站立、RL 行走、零力矩；
- 机体速度控制：`vx`、`vy`、`wz`和持续时间；
- 高层运行状态；
- 12个电机和4路通信端口的只读状态。

本目录不包含机器狗服务端、LCM、ONNX模型、电机SDK、底层LowCmd接口及关节控制命令。

## 环境

Windows推荐使用WSL2。Linux可以直接使用。

```bash
sudo apt update
sudo apt install -y build-essential cmake
```

## 编译

在本目录执行：

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

输出：

```text
build/libfengfu_loco_sdk.a
build/bin/ai_loco_tool
build/bin/loco_move_demo
```

## 连接

网线直连时：

```text
机器狗：192.168.10.125/24
电脑：192.168.10.10/24
端口：9001
```

```bash
./build/bin/ai_loco_tool 192.168.10.125 state
./build/bin/ai_loco_tool 192.168.10.125 console
```

完整接口、状态字段、电机字段和安全说明见：

```text
FENGFU_LOCO_SDK(SDK文档必读).md
```
