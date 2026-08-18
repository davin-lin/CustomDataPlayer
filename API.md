# API 文档

本文档描述 CustomData 项目的对外 API，供上层调用方参考。模块内部私有成员不在本文档范围内。

***

## 目录

- [1. Player（播放器主控）](#1-player播放器主控)
- [2. DataPlayer（数据流播放器）](#2-dataplayer数据流播放器)
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
void Start();                      // 启动所有子线程（demux/decode/audio/data）
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
player.Start();   // 启动 demux / decode / audio / data 线程

EventLoop loop;
loop.Run(player);   // 阻塞直到用户关闭窗口

player.Close();
```

***

## 2. DataPlayer（数据流播放器）

**头文件**：`AVPlayer/include/data_player.h`

继承 `ThreadBase`，在独立线程中从 `dataPacketQueue_` 取 DATA 包（JSON 格式），解析后按 PTS 推入 `dataFrameQueue_`，供 `VideoPlayer` 同步显示。

### 接口

```cpp
DataPlayer(std::shared_ptr<Context> ctx);
~DataPlayer();

int  Open();    // 查找 DATA 流（AVMEDIA_TYPE_DATA + BIN_DATA），返回 0 成功，-1 无数据流
int  Start();   // 启动线程
int  Close();   // 停止线程 + 清空队列
```

### 数据格式

DATA 流每个包的 payload 为 JSON 文本，支持以下字段：

| 字段 | 类型 | 含义 |
|---|---|---|
| `frame` | int64 | 视频帧序号 |
| `pts` | int64 | 视频原始 pts（参考用） |
| `time_ms` | int64 | 墙钟时间（ms），同步主基准 |
| `data_id` | int64 | 数据分组 ID（去重用） |
| `value` | int64 | 业务值 |
| `speed` | double | 速度 |
| `temperature` | double | 温度 |
| `message` | string | 消息文本 |
| `longitude` | double | 经度 |
| `latitude` | double | 纬度 |

### 同步与去重

- **时间基准**：优先用 `time_ms / 1000.0`（与 `videoClock_` 同基），回退用 `pts * time_base`
- **去重**：相同 `data_id` 的包只保留第一帧，防 overlay 跳变
- **输出**：解析后构建 7 行文本（ID/Value/Speed/Temp/Msg/Lon/Lat），推入 `dataFrameQueue_`

### 使用流程

```cpp
// 由 Player::Open / Start / Close 自动管理
// Player::Open() → dataPlayer_->Open()
// Player::Start() → dataPlayer_->Start()
// Player::Close() → dataPlayer_->Close()
```

***

## 3. TTFRenderer（文字渲染器）

**头文件**：`AVPlayer/include/ttf_renderer.h`

封装 SDL_ttf，负责把 DATA 流文本行渲染成半透明叠加层，绘制到 SDL Renderer 上右上角位置。采用脏标志 + Texture 缓存机制，仅在内容变化时重建 Texture。

### 接口

```cpp
TTFRenderer();
~TTFRenderer();   // 自动调用 Destroy()

int  Init();       // 初始化 TTF 库并加载字体，返回 0 成功
void Destroy();    // 释放字体和叠加纹理

// 设置 DATA 流叠加层文本行（每帧由 UpdateDataOverlay 调用）
// lines: 每行一个字符串（ID/Value/Speed/Temp/Msg/Lon/Lat）
// 内容未变时直接 return，不置脏
void SetStreamLines(const std::vector<std::string>& lines);

// 在 renderer 上绘制叠加层（每帧 Display 末尾调用）
void RenderStreamOverlay(SDL_Renderer* renderer);
```

### 使用流程

```cpp
TTFRenderer renderer;
renderer.Init();

// 运行期每帧：
videoPlayer.Refresh();    // 内部自动调用 UpdateDataOverlay → SetStreamLines
// Display() 末尾自动调用 RenderStreamOverlay(rendererPtr)

// 退出时：
renderer.Destroy();   // 或直接析构
```

### 字体加载策略

`Init()` 内部按以下顺序尝试加载字体（24px）：

1. `assets/fonts/simhei.ttf`（项目内打包，推荐）
2. `C:\Windows\Fonts\simhei.ttf`
3. `C:\Windows\Fonts\msyh.ttc`
4. `C:\Windows\Fonts\arial.ttf`

任一加载成功即停止，全部失败时 `RenderStreamOverlay` 静默不绘制。

### 性能说明

- `SetStreamLines` 仅在内容变化时置 `streamDirty_ = true`；超大输入（>64 行或单行 >1024 字节）直接 return 不重建
- `RenderStreamOverlay` 仅在 dirty 时调用 `BuildStreamTexture` 重建 Texture，其余帧只做 `SDL_RenderCopy`
- 叠加层位置固定为右上角 `{logicalW - streamW_ - 10, 10}`（logical 优先，fallback 到 output size），背景半透明黑色 (RGBA 0,0,0,160)

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

`VideoPlayer` 在每帧 `Refresh()` → `Display()` 中调用 `UpdateDataOverlay(vp)`，以 ffplay 字幕式同步算法从 `dataFrameQueue_` 取当前生效的 DATA 帧，通过 `ttfRenderer_.SetStreamLines()` 下发文本行，再由 `RenderStreamOverlay()` 绘制。无需在 `Open()` 时一次性设置。

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

### DATA 流相关字段

```cpp
int             dataIndex_ = -1;         // DATA 流索引（-1 表示无）
AVStream*       dataStream_ = nullptr;    // DATA 流描述
PacketQueue     dataPacketQueue_;         // DATA 包队列
FrameQueue      dataFrameQueue_;         // DATA 帧队列
```

**写入时机**：`Demuxer::DemuxLoop()` 分发 DATA 包到 `dataPacketQueue_`；`DataPlayer` 线程取包解析后推入 `dataFrameQueue_`。

**读取时机**：`VideoPlayer::UpdateDataOverlay()` 从 `dataFrameQueue_` 取当前生效帧。

### 其他关键字段

| 字段                                        | 类型                 | 说明           |
| ----------------------------------------- | ------------------ | ------------ |
| `fmtCtx_`                                 | `AVFormatContext*` | FFmpeg 格式上下文 |
| `paused_`                                 | `atomic<bool>`     | 暂停标志         |
| `stop_`                                   | `atomic<bool>`     | 停止标志         |
| `forceRefresh_`                           | `int`              | 强制刷新标志       |
| `audioPacketQueue_` / `videoPacketQueue_` / `dataPacketQueue_` | `PacketQueue` | 包队列          |
| `audioFrameQueue_` / `videoFrameQueue_` / `dataFrameQueue_` | `FrameQueue`       | 帧队列          |
| `audioClock_` / `videoClock_`             | `Clock`            | 时钟同步         |
| `masterClock_`                            | `Clock*`           | 主时钟（默认音频时钟）  |

### 注意

`Context` 通过 `friend class` 开放访问给 `Player`、`Demuxer`、`Decoder`、`AudioDecoder`、`VideoDecoder`、`AudioPlayer`、`VideoPlayer`、`DataPlayer`。外部代码**不应**直接访问其 private 字段，应通过 `Player` 的公开接口。

***

## 典型调用时序

```
1. Player::Open(filename)
   ├── new Context(filename)
   ├── Demuxer::Open()             → avformat_open_input + find_stream_info
   ├── AudioDecoder::Open()
   ├── VideoDecoder::Open()
   ├── AudioPlayer::Open()         → SDL_OpenAudioDevice
   ├── VideoPlayer::Open()
   │   └── SDL_CreateWindow/Renderer
   │       └── TTFRenderer::Init() → TTF_Init + LoadFont
   └── DataPlayer::Open()          → 查找 DATA 流（无则跳过）

2. Player::Start()
   ├── Demuxer::Start()           → DemuxLoop 线程
   ├── AudioDecoder::Start()
   ├── VideoDecoder::Start()
   ├── AudioPlayer::Start()       → SDL_PauseAudioDevice(0)
   └── DataPlayer::Start()        → DATA 解析线程

3. EventLoop::Run(player)         → 阻塞处理 SDL 事件

4. 每帧视频刷新（VideoPlayer::Refresh → Display）
   ├── Render()                   → SDL_RenderCopy 视频帧
   ├── UpdateDataOverlay(vp)     → 从 dataFrameQueue_ 取当前帧 → SetStreamLines
   ├── RenderStreamOverlay()      → 叠加 DATA 文本（脏标志 + Texture 缓存）
   └── SDL_RenderPresent()

5. Player::Close()
   ├── VideoPlayer::Close()       → TTFRenderer::Destroy()
   ├── AudioPlayer::Close()
   ├── DataPlayer::Close()
   ├── AudioDecoder::Close()
   ├── VideoDecoder::Close()
   ├── Demuxer::Close()
   └── SDL_Quit()
```

