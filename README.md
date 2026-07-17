# elderly_care — AI 智能养老看护终端

## 一、作品简介

面向独居老人/养老机构场景的 **AI 智能养老看护终端**，基于 OpenVela/NuttX RTOS 实现。通过摄像头人体/跌倒检测、语音呼救识别、环境温湿度监测多模态融合，在异常发生时触发本地蜂鸣+LED 告警、LVGL 屏幕状态显示，并通过 MQTT 上报至云端，同时具备长时间无人时自动低功耗的能力。

**核心亮点：**

- **多模态感知**：视觉（V4L2 摄像头 + DSP 推理）+ 听觉（PCM 音频 + RMS/burst 检测）+ 环境（uORB 温湿度）三路并行
- **三级告警**：INFO / WARNING / CRITICAL，PWM 蜂鸣器不同频率 + GPIO LED
- **本地显示**：LVGL 实时展示温湿度、人体状态、告警计数、WiFi、运行时长
- **云端联动**：手写 MQTT 协议，JSON 上报到 broker.emqx.io，无需外部 MQTT 库
- **低功耗**：NuttX PM API，30 分钟无人检测自动进入低功耗，唤醒后恢复
- **10 模块化设计**：每个功能独立模块，Kconfig 开关按需启用，适配资源受限设备

## 二、选题方向

**AI 硬件产品创新**。选择该方向是因为养老看护是 AIoT 的典型落地场景，需要同时具备边缘 AI 推理、多传感器融合、实时告警、低功耗、网络上报等能力，能充分体现 OpenVela RTOS 在嵌入式 AI 硬件上的综合优势。

## 三、目录结构

```text
contest2026_118_ganfanbuxiangpaidui/
├── app/
│   ├── elderly_care/                 # 参赛作品：AI 养老看护终端
│   │   ├── src/
│   │   │   ├── elderly_care.h        # 公共头：常量/事件/数据结构/函数原型
│   │   │   ├── main.c                # 主控：5 个 pthread + 互斥锁 + 信号优雅退出
│   │   │   ├── camera_module.c       # V4L2 采集 240×320 YUYV
│   │   │   ├── dsp_inference.c       # 人/跌倒检测，YUV→RGB→96×96，无模型帧差法 fallback
│   │   │   ├── audio_module.c        # 16kHz/2ch/16bit PCM，RMS+burst 检测呼救
│   │   │   ├── sensor_module.c       # uORB 温湿度，无硬件走 mock
│   │   │   ├── alert_module.c        # PWM 蜂鸣器 + GPIO LED，三级告警
│   │   │   ├── display_module.c      # LVGL 显示温湿度/人体/警报/WiFi/运行时长
│   │   │   ├── wifi_module.c         # 手写 MQTT 协议，JSON 发布到 broker.emqx.io:1883
│   │   │   ├── power_module.c        # NuttX PM API，30 分钟无人低功耗
│   │   │   └── log_module.c          # 环形缓冲 + Flash 写回
│   │   ├── Make.defs                 # Make 构建系统注册
│   │   └── CMakeLists.txt            # CMake 构建系统注册
│   └── hello_app/                    # 组委会样例骨架（可忽略）
├── quickapp/hello_quickapp/          # 组委会样例骨架（可忽略）
├── board/contest_board/              # 组委会样例骨架（可忽略）
├── logs/                             # AI Coding 日志
├── contest2026_118_ganfanbuxiangpaidui.xml   # repo manifest（含 elderly_care linkfile 映射）
└── README.md                         # 本文件
```

`contest2026_118_ganfanbuxiangpaidui.xml` 中的 `<linkfile>` 会把 `app/elderly_care` 软链到 openvela 编译树的 `packages/demos/contest2026_118_elderly_care`，**生产仓库零改动**。

## 四、运行方式

### 1. 拉取完整工程

```bash
repo init -u https://github.com/open-vela/contest2026_118_ganfanbuxiangpaidui \
  -b dev-ai-contest-2026 -m contest2026_118_ganfanbuxiangpaidui.xml
repo sync -c -j8
```

### 2. 编译固件

在 openvela 工作区根目录（仓库上一级）执行：

```bash
cd ..
# goldfish QEMU 模拟器 board config
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake -j8
```

如需启用 elderly_care 各模块，通过 `menuconfig` 开启：

```bash
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap menuconfig
```

路径：`Application Configuration → Demos → Contest 2026 Team 118 → Elderly Care`，打开主开关 `CONFIG_CONTEST2026_118_ELDERLY_CARE` 及所需子模块开关（摄像头/DSP/音频/传感器/告警/显示/WiFi/低功耗/日志），保存后重新编译。

### 3. 运行（QEMU goldfish 模拟器）

编译产物在 `nuttx/` 下：`nuttx`、`nuttx.bin`。使用 OpenVela 提供的 emulator 启动：

```bash
emulator -vela -avd Vela_Generic_Device -read-only -no-window \
  -shell-serial tcp::4444,server,nowait ...
```

NuttX 启动后在 NSH 中运行：

```bash
nsh> elderly_care
```

### 4. 模块说明

| 模块 | 设备节点 | 说明 |
|------|----------|------|
| camera | /dev/video0 | V4L2 采集 240×320 YUYV |
| dsp_inference | — | YUV→RGB→96×96 缩放，无模型时帧差法 fallback |
| audio | /dev/audio/pcm0c | 16kHz/2ch/16bit PCM，RMS+burst 检测呼救 |
| sensor | uORB | 温湿度，无硬件走 mock |
| alert | /dev/pwm0, /dev/gpio0 | PWM 蜂鸣器 + GPIO LED，三级告警 |
| display | LVGL | 温湿度/人体/警报/WiFi/运行时长 |
| wifi | — | 手写 MQTT，JSON 发布到 broker.emqx.io:1883 |
| power | NuttX PM | 30 分钟无人低功耗 |
| log | /dev/elog | 环形缓冲 + Flash 写回 |

## 五、AI Coding 使用说明

本作品在 AI 辅助下完成开发，协作环节如下：

- **需求拆解**：与 AI 讨论养老看护场景的痛点，AI 协助把功能拆解为摄像头、音频、传感器、告警、显示、网络、低功耗、日志 10 个独立模块，并定义模块间数据结构（`elderly_care.h` 中的 `system_state_s`、事件枚举、告警级别）。
- **方案设计**：AI 协助设计 5 线程主控架构（AI/音频/传感器/显示/电源），用 `pthread_mutex_t` 保护共享状态，`signal` 实现优雅退出；MQTT 协议因资源受限选择手写而非引入外部库。
- **编码**：AI 生成各模块代码框架，人工审查并调整 API 细节（如 V4L2 ioctl 序列、uORB 订阅、LVGL 控件布局）。
- **调试**：编译验证阶段，AI 协助定位并修复 API 兼容性问题——LVGL v7/v8 → v9（`lv_label_create` 去 copy 参数、`lv_obj_align` 去 base 参数改用 `lv_obj_align_to`、`LV_ALIGN_IN_TOP_LEFT` 改名）、NuttX `audio_caps_s` 结构体成员变更（sample rate/bit width 改走 `ac_controls`）、`GPIOC_WRITE` 需 `CONFIG_DEV_GPIO` 条件编译。
- **文档**：AI 协助编写本 README、代码注释和提交信息。

完整对话日志见 `logs/` 目录。

## 附：构建系统说明

elderly_care 同时支持 Make 和 CMake 两种构建系统：

- **Make**：`Make.defs` 通过 `CONFIGURED_APPS` 注册到 `packages/demos`
- **CMake**：`CMakeLists.txt` 通过 `nuttx_add_application` 注册，按 Kconfig 开关条件收集源文件

两者使用相同的 Kconfig 开关（`CONFIG_CONTEST2026_118_ELDERLY_CARE` 主开关 + 9 个子模块开关