# API 文档

本文档描述 CustomData 项目的对外 API，供上层调用方参考。模块内部私有成员不在本文档范围内。

***

## 目录

- [1. Player（播放器主控）](#1-player播放器主控)
- [2. CustomData 自定义数据解析](#2-customdata-自定义数据解析)
- [3. TTFRenderer（文字渲染器）](#3-ttfrenderer文字渲染器)
- [4. VideoPlayer（视频渲染）](#4-videoplayer视频渲染)
- [5. AudioPlayer（音频播放）](#5-audioplayer音频播放)
- [6. Demuxer（解复用）](#6-demuxer解复用)
- [7. Decoder（解码器基类）](#7-decoder解码器基类)
- [8. EventLoop（事件循环）](#8-eventloop事件循环)
- [9. Context（共享上下文）](#9-context共享上下文)

***

## 1. Player（播放器主控）

**头文件**：`AVPlayer/include/player.h`

播放器的统一入口，负责组装解复用、解码、音视频播放等子模块，并向外暴露播放控制接口。

### 构造与生命周期

```cpp
Player();
int  Open(const char* filename);  // 打开媒体文件，返回 0 成功，<0 失败
void Start();                      // 启动所有子线程（demux/decode/audio）
void Close();                      // 关闭并释放所有资源
```

**调用顺序**：

```cpp
Player player;
player.Open("xxx.mp4");
player.Start();
// ... 事件循环 ...
player.Close();
```

### 播放控制

| 方法                                                        | 说明                         |
| --------------------------------------------------------- | -------------------------- |
| `void Refresh()`                                          | 触发一次视频刷新                   |
| `void ForceRefresh()`                                     | 强制刷新（设置 forceRefresh\_ 标志） |
| `void TogglePause()`                                      | 暂停 / 恢复                    |
| `void ToggleMute()`                                       | 静音 / 取消静音                  |
| `void ToggleFullScreen()`                                 | 全屏切换                       |
| `void UpdateWidthHeight(int w, int h)`                    | 通知窗口尺寸变化                   |
| `void VolumeUp(int volume)`                               | 音量增加（增量）                   |
| `void VolumeDown(int volume)`                             | 音量减少（减量）                   |
| `void SeekForward(double incr, bool seekByBytes = true)`  | 前进，incr 单位秒                |
| `void SeekBackward(double incr, bool seekByBytes = true)` | 后退，incr 单位秒                |

### 状态查询

```cpp
bool IsPaused() const;  // 是否暂停
bool IsMuted() const;   // 是否静音
```

### 示例

```cpp
Player player;
if (player.Open(DEFAULT_MEDIA_PATH) != 0) return -1;
player.Start();

EventLoop loop;
loop.Run(player);   // 阻塞直到用户关闭窗口

player.Close();
```

***

## 2. CustomData 自定义数据解析

**头文件**：`CustomMetadata/custom_data.h`

从 MP4 全局 metadata 读取自定义数据，支持 JSON 和 Protobuf 两种格式，通过宏 `CUSTOM_DATA_FORMAT` 切换。

### 数据结构

```cpp
struct CustomData {
    bool        hasData;      // 是否成功解析到数据
    std::string usrName;      // 用户名
    std::string usrCompany;   // 公司
    std::string usrType;      // 类型标识（"JSON" 或 "Protobuf"）
};
```

### 格式切换宏

```cpp
// 在 custom_data.h 顶部修改
#define CUSTOM_DATA_FORMAT 1   // 1 = JSON
#define CUSTOM_DATA_FORMAT 2   // 2 = Protobuf
```

| 值   | 解析函数                          | metadata key           | 默认测试文件                      |
| --- | ----------------------------- | ---------------------- | --------------------------- |
| `1` | `ParseCustomDataFromJson`     | `video_custom_data`    | `walking-dead-json.mp4`     |
| `2` | `ParseCustomDataFromProtobuf` | `video_custom_pb_data` | `walking-dead-protobuf.mp4` |

### 解析函数

```cpp
// 统一入口（推荐），由宏自动分发
CustomData ParseCustomData(AVFormatContext* fmtCtx);

// 直接调用（不常用）
CustomData ParseCustomDataFromJson(AVFormatContext* fmtCtx);
CustomData ParseCustomDataFromProtobuf(AVFormatContext* fmtCtx);
```

**参数**：`fmtCtx` —— 已通过 `avformat_open_input` + `avformat_find_stream_info` 打开的格式上下文。

**返回值**：解析失败时 `hasData = false`，三个字段为空字符串。

### JSON 格式约定

metadata value 是 JSON 字符串：

```json
{
    "usr_name": "linmingyang",
    "usr_company": "OBSBOT",
    "usr_type": "JSON"
}
```

### Protobuf 格式约定

metadata value 是 `Usr` 消息的 `SerializeAsString()` 二进制串（写入端以字符串形式存储）。proto 定义见 `CustomMetadata/usr.proto`：

```proto
message Usr {
    string name    = 1;
    string company = 2;
    string type    = 3;
}
```

### 默认媒体路径宏

切换格式时，默认播放文件也会自动切换：

```cpp
DEFAULT_MEDIA_PATH   // 由 CUSTOM_DATA_FORMAT 自动定义为对应 mp4 路径
```

***

## 3. TTFRenderer（文字渲染器）

**头文件**：`AVPlayer/include/ttf_renderer.h`

封装 SDL\_ttf，负责把自定义数据渲染成半透明叠加层，绘制到 SDL Renderer 上。

### 接口

```cpp
TTFRenderer();
~TTFRenderer();   // 自动调用 Destroy()

int  Init();       // 初始化 TTF 库并加载字体，返回 0 成功
void Destroy();    // 释放字体和叠加纹理

void SetOverlayData(const std::string& name,
                    const std::string& company,
                    const std::string& type);   // 设置要显示的文本

void RenderOverlay(SDL_Renderer* renderer);     // 在 renderer 上绘制叠加层
```

### 使用流程

```cpp
TTFRenderer renderer;
renderer.Init();
renderer.SetOverlayData("linmingyang", "OBSBOT", "JSON");

// 在每帧渲染视频后调用：
renderer.RenderOverlay(rendererPtr);

// 退出时：
renderer.Destroy();   // 或直接析构
```

### 字体加载策略

`Init()` 内部按以下顺序尝试加载字体（24px）：

1. `assets/fonts/simhei.ttf`（项目内打包，推荐）
2. `d:\software\VisualStudio\code\CustomData\assets\fonts\simhei.ttf`
3. `C:\Windows\Fonts\simhei.ttf`
4. `C:\Windows\Fonts\msyh.ttc`
5. `C:\Windows\Fonts\arial.ttf`

任一加载成功即停止，全部失败时 `RenderOverlay` 静默不绘制。

### 性能说明

- `SetOverlayData` 仅在内容变化时置 `dataDirty_ = true`
- `RenderOverlay` 仅在 dirty 时重建 Texture，其余帧只做 `SDL_RenderCopy`
- 叠加层位置固定为左上角 (10, 10)，背景半透明黑色 (RGBA 0,0,0,160)

***

## 4. VideoPlayer（视频渲染）

**头文件**：`AVPlayer/include/video_player.h`

负责 SDL 窗口、视频帧渲染，并在每帧渲染后调用 `TTFRenderer` 绘制叠加层。

### 接口

```cpp
VideoPlayer(std::shared_ptr<Context> ctx);
int  Open();    // 创建窗口/Renderer/Texture，初始化 TTFRenderer
int  Start();   // 启动刷新定时器
int  Close();   // 销毁窗口和 SDL 资源

void UpdateWidthHeight(int width, int height);
void ToggleFullScreen();
int  Run(int interval);   // 单次刷新（由定时器或外部触发）
```

### 默认窗口尺寸

```cpp
#define SDL_WINDOW_DEFAULT_WIDTH  1080
#define SDL_WINDOW_DEFAULT_HEIGHT 720
```

### 叠加层数据来源

`VideoPlayer` 在 `Open()` 时从 `Context` 读取自定义数据并传给 `TTFRenderer`：

```cpp
ttfRenderer_.SetOverlayData(ctx_->usrName_, ctx_->usrCompany_, ctx_->usrType_);
```

***

## 5. AudioPlayer（音频播放）

**头文件**：`AVPlayer/include/audio_player.h`

基于 SDL\_audio 的音频输出模块。

### 接口

```cpp
AudioPlayer(std::shared_ptr<Context> ctx);
int  Open();    // 打开音频设备
int  Start();   // 开始播放
int  Stop();    // 停止播放
int  Close();   // 关闭音频设备

void ToggleMute();          // 静音切换
void UpdateVolume(int volume);  // 调整音量（正数增加，负数减少）
bool IsMuted() const;       // 是否静音
```

### 音量范围

```cpp
#define MIN_VOLUME_VALUE     0
#define DEFAULT_VOLUME_VALUE 64
#define MAX_VOLUME_VALUE     128
```

***

## 6. Demuxer（解复用）

**头文件**：`AVPlayer/include/demuxer.h`

继承 `ThreadBase`，在独立线程中读取媒体包并分发到对应队列。

### 接口

```cpp
Demuxer(std::shared_ptr<Context> ctx);
int  Open();    // 打开文件 + 查找流信息 + 打开编解码器
int  Close();
void Seek(double incr, int seekByBytes);  // seek，incr 单位秒
void Run() override;                       // 线程入口
```

***

## 7. Decoder（解码器基类）

**头文件**：`AVPlayer/include/decoder.h`

`AudioDecoder` 和 `VideoDecoder` 的基类，继承 `ThreadBase`。

### 接口

```cpp
Decoder(std::shared_ptr<Context> ctx, AVMediaType mediaType);
int Open();
int Close();
int Decode(AVCodecContext* codecCtx, AVFrame* frame);  // 解码一个包
```

**子类**：

- `AudioDecoder`（`AVPlayer/include/audio_decoder.h`）
- `VideoDecoder`（`AVPlayer/include/video_decoder.h`）

构造时传入 `AVMEDIA_TYPE_AUDIO` / `AVMEDIA_TYPE_VIDEO`。

***

## 8. EventLoop（事件循环）

**头文件**：`AVPlayer/include/event_loop.h`

处理 SDL 窗口事件（键盘、窗口关闭、尺寸变化等），把事件转换为 `Player` 的方法调用。

### 接口

```cpp
EventLoop();
int Run(Player& player);   // 阻塞事件循环，直到窗口关闭
```

### 支持的按键

| 按键      | 动作          |
| ------- | ----------- |
| `空格`    | 暂停 / 恢复     |
| `M`     | 静音切换        |
| `F`     | 全屏切换        |
| `↑ / ↓` | 音量增减        |
| `← / →` | 后退 / 前进 10s |

> 具体绑定见 `AVPlayer/src/event_loop.cpp`。

***

## 9. Context（共享上下文）

**头文件**：`AVPlayer/include/context.h`

所有模块共享的中心状态对象，由 `std::shared_ptr` 持有。大部分字段为 private，通过 friend 关系开放给各子模块。

### 自定义数据相关字段

```cpp
bool        hasCustomData_ = false;  // 是否有自定义数据
std::string usrName_;                // 用户名
std::string usrCompany_;             // 公司
std::string usrType_;                // 类型
```

**写入时机**：`Player::Open()` 在 `demuxer_->Open()` 之后调用 `ParseCustomData()` 写入。

**读取时机**：`VideoPlayer::Open()` 读取并传给 `TTFRenderer`。

### 其他关键字段

| 字段                                        | 类型                 | 说明           |
| ----------------------------------------- | ------------------ | ------------ |
| `fmtCtx_`                                 | `AVFormatContext*` | FFmpeg 格式上下文 |
| `paused_`                                 | `atomic<bool>`     | 暂停标志         |
| `stop_`                                   | `atomic<bool>`     | 停止标志         |
| `forceRefresh_`                           | `int`              | 强制刷新标志       |
| `audioPacketQueue_` / `videoPacketQueue_` | `PacketQueue`      | 包队列          |
| `audioFrameQueue_` / `videoFrameQueue_`   | `FrameQueue`       | 帧队列          |
| `audioClock_` / `videoClock_`             | `Clock`            | 时钟同步         |
| `masterClock_`                            | `Clock*`           | 主时钟（默认音频时钟）  |

### 注意

`Context` 通过 `friend class` 开放访问给 `Player`、`Demuxer`、`Decoder`、`AudioDecoder`、`VideoDecoder`、`AudioPlayer`、`VideoPlayer`。外部代码**不应**直接访问其 private 字段，应通过 `Player` 的公开接口。

***

## 典型调用时序

```
1. Player::Open(filename)
   ├── new Context(filename)
   ├── Demuxer::Open()             → avformat_open_input + find_stream_info
   ├── ParseCustomData(fmtCtx)     → ctx_->usrName_/usrCompany_/usrType_
   ├── AudioDecoder::Open()
   ├── VideoDecoder::Open()
   ├── AudioPlayer::Open()         → SDL_OpenAudioDevice
   └── VideoPlayer::Open()
       ├── SDL_CreateWindow/Renderer
       ├── TTFRenderer::Init()     → TTF_Init + LoadFont
       └── TTFRenderer::SetOverlayData(ctx_->usrName_, ...)

2. Player::Start()
   ├── Demuxer::Start()           → DemuxLoop 线程
   ├── AudioDecoder::Start()
   ├── VideoDecoder::Start()
   └── AudioPlayer::Start()       → SDL_PauseAudioDevice(0)

3. EventLoop::Run(player)         → 阻塞处理 SDL 事件

4. 每帧视频刷新（VideoPlayer::Display）
   ├── Render()                   → SDL_RenderCopy 视频帧
   ├── TTFRenderer::RenderOverlay()  → 叠加自定义数据
   └── SDL_RenderPresent()

5. Player::Close()
   ├── VideoPlayer::Close()       → TTFRenderer::Destroy()
   ├── AudioPlayer::Close()
   ├── AudioDecoder::Close()
   ├── VideoDecoder::Close()
   ├── Demuxer::Close()
   └── SDL_Quit()
```

