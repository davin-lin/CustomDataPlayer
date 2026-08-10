# CustomData

基于 SDL + FFmpeg 的视频播放器，支持从 MP4 metadata 中读取自定义数据（JSON 或 Protobuf）并以文字叠加层的形式显示在视频画面上。

## 编译

1. 用 Visual Studio 打开 `CustomData.sln`
2. 选择 `Debug | x64` 配置
3. F5 编译运行

> 第三方依赖（SDL、SDL_ttf、FFmpeg、Protobuf、nlohmann/json）已包含在项目目录中，无需额外安装。

## 切换数据格式

在 `CustomMetadata/custom_data.h` 中修改宏定义：

```cpp
#define CUSTOM_DATA_FORMAT 1   // 1 = JSON，   默认播放 walking-dead-json.mp4
#define CUSTOM_DATA_FORMAT 2   // 2 = Protobuf，默认播放 walking-dead-protobuf.mp4
```

修改后重新编译即可，解析入口 `ParseCustomData()` 会自动分发到对应的解析函数，播放器其余逻辑不变。

## 自定义数据结构

两种格式存储的字段一致：`usr_name`、`usr_company`、`usr_type`。

### JSON 格式
- metadata key：`video_custom_data`
- 存储内容：JSON 字符串
```json
{
    "usr_name": "linmingyang",
    "usr_company": "OBSBOT",
    "usr_type": "JSON"
}
```

### Protobuf 格式
- metadata key：`video_custom_pb_data`
- 定义见 `CustomMetadata/usr.proto`
```proto
message Usr {
    string name    = 1;
    string company = 2;
    string type    = 3;
}
```

## 字体

文字叠加层使用 SDL_ttf 渲染，字体加载优先级：

1. `assets/fonts/simhei.ttf`（项目内打包，推荐）
2. 项目根目录 `simhei.ttf`
3. `C:\Windows\Fonts\simhei.ttf`
4. `C:\Windows\Fonts\msyh.ttc`
5. `C:\Windows\Fonts\arial.ttf`

如需跨平台部署，建议把 `simhei.ttf` 放入 `assets/fonts/` 目录。

## 文件结构

```
CustomData/
├── main.cpp                      # 入口，调用 Player 播放视频
├── CustomData.sln / .vcxproj     # VS 工程
├── walking-dead-json.mp4         # JSON 格式测试视频
├── walking-dead-protobuf.mp4     # Protobuf 格式测试视频
│
├── CustomMetadata/               # 自定义数据解析模块
│   ├── custom_data.h             # 格式切换宏 + ParseCustomData() 统一入口
│   ├── custom_data.cpp           # JSON / Protobuf 两种解析实现
│   ├── json.hpp                  # nlohmann/json 头文件
│   ├── usr.proto                 # Protobuf 协议定义
│   ├── usr.pb.h / usr.pb.cc      # Protobuf 生成代码
│   └── json_data.h / proto_data.h # 元数据写入工具（参考用）
│
├── AVPlayer/                     # 播放器核心
│   ├── include/
│   │   ├── player.h              # 播放器主类
│   │   ├── context.h             # 共享上下文（含自定义数据字段）
│   │   ├── demuxer.h             # 解复用
│   │   ├── decoder.h / video_decoder.h / audio_decoder.h
│   │   ├── video_player.h        # 视频渲染（叠加层调用点）
│   │   ├── audio_player.h        # 音频播放
│   │   ├── ttf_renderer.h       # SDL_ttf 文字渲染封装
│   │   ├── frame_queue.h / packet_queue.h
│   │   ├── clock.h
│   │   └── event_loop.h
│   └── src/                      # 对应实现
│
├── SDL/                          # SDL2 库
├── SDL_ttf/                      # SDL2_ttf 库
├── ffmpeg/                       # FFmpeg 库
├── protobuf/                     # Protobuf 库
└── SDL2.dll / SDL2_ttf.dll       # 运行时动态库
```
