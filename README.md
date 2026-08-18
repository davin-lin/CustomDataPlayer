# CustomDataAVPlayer

基于 SDL + FFmpeg 的视频播放器，支持 MP4 中的 **DATA 流**（JSON 格式）以文字叠加层形式与音视频同步显示。

## 相关文档

- [API 文档](API.md)：各模块（Player、Demuxer、Decoder、VideoPlayer、AudioPlayer、DataPlayer、TTFRenderer、EventLoop、Context）的对外接口与调用时序说明
- [设计文档](Design.md)：播放器整体架构、类图、模块关系与数据流设计

## 编译

1. 用 Visual Studio 打开 `CustomData.sln`
2. 选择 `Debug | x64` 配置
3. F5 编译运行

> 第三方依赖（SDL、SDL\_ttf、FFmpeg）已包含在项目目录中，无需额外安装。Protobuf 仅保留头文件，运行时不需要。

## 自定义数据流（DATA Stream）

叠加层文本来源是 MP4 中的一条 DATA 流（stream type = `AVMEDIA_TYPE_DATA`），按视频 PTS 同步显示，支持 seek/pause 后正确对齐。

### 数据格式

每条数据包的 payload 是 JSON 字符串，字段：

```json
{
  "pts": 10.5,
  "duration": 2.0,
  "lines": [

  ]
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `pts` | double | 该条数据应显示的视频 PTS（秒） |
| `duration` | double | 持续显示时长（秒） |
| `lines` | string[] | 显示的文本行，按顺序从上到下渲染 |

### 生成含 DATA 流的 MP4

参考脚本 `CustomMetadata/json_stream_data.h` 中的 `CreateJsonStreamMP4()`，流程：

1. 打开原始视频（H.264 + AAC）
2. 新增 `AVMEDIA_TYPE_DATA` 流，codec_id = `AV_CODEC_ID_JSON`
3. 读取 JSON 数据文件，按 pts 写入 AVPacket 到 DATA 流
4. 复用音视频流到输出文件

生成后播放：`main.cpp` 会自动检测 DATA 流并启动 DataPlayer 线程解包渲染。

## 字体

文字叠加层使用 SDL\_ttf 渲染，字体加载优先级：

1. `assets/fonts/simhei.ttf`（项目内打包，推荐）
2. 项目根目录 `simhei.ttf`
3. `C:\Windows\Fonts\simhei.ttf`
4. `C:\Windows\Fonts\msyh.ttc`
5. `C:\Windows\Fonts\arial.ttf`

如需跨平台部署，建议把 `simhei.ttf` 放入 `assets/fonts/` 目录。

## 音视频同步关键参数

在 `AVPlayer/include/opts.h` 中定义：

| 宏 | 默认值 | 说明 |
|---|---|---|
| `g_framedrop` | `-1` | 丢帧策略：-1=ffplay默认（非视频主同步时丢），0=不丢，>0=强制丢 |
| `AV_SYNC_THRESHOLD_MIN` | `0.04` | 音视频同步微调下限（s） |
| `AV_SYNC_THRESHOLD_MAX` | `0.1` | 音视频同步微调上限，也是 snap 重置阈值 |
| `AV_SYNC_FRAMEDUP_THRESHOLD` | `0.1` | 帧持续时长超过此值时不做帧复制补偿 |

## 文件结构

```
CustomData/
├── main.cpp                      # 入口，调用 Player 播放视频
├── CustomData.sln / .vcxproj     # VS 工程
├── walking-dead-json-stream.mp4  # 含 DATA 流的测试视频
│
├── CustomMetadata/               # 数据写入工具（参考，不参与编译运行）
│   ├── json.hpp                  # nlohmann/json 头文件
│   └── json_stream_data.h        # CreateJsonStreamMP4() 写入脚本
│
├── AVPlayer/                     # 播放器核心
│   ├── include/
│   │   ├── player.h              # 播放器主类
│   │   ├── context.h             # 共享上下文（streamOverlayLines_ 等）
│   │   ├── demuxer.h             # 解复用（含 DATA 流分流）
│   │   ├── decoder.h / video_decoder.h / audio_decoder.h
│   │   ├── video_player.h        # 视频渲染 + 调度 + RenderStreamOverlay
│   │   ├── audio_player.h        # 音频播放 + SDL callback
│   │   ├── data_player.h         # DATA 流解包 → streamOverlayLines_
│   │   ├── ttf_renderer.h        # SDL_ttf 文字渲染封装（流式）
│   │   ├── frame_queue.h / packet_queue.h
│   │   ├── clock.h
│   │   └── event_loop.h
│   └── src/                      # 对应实现
│
├── SDL/                          # SDL2 库
├── SDL_ttf/                      # SDL2_ttf 库
├── ffmpeg/                       # FFmpeg 库
├── protobuf/                     # Protobuf 头文件（保留，运行时不依赖）
└── SDL2.dll / SDL2_ttf.dll       # 运行时动态库
```
