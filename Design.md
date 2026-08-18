# AVPlayer

基于 FFmpeg + SDL2 的跨平台音视频播放器，架构参考 ffplay 实现。

## 类图

```mermaid
classDiagram
    direction TB

    class ThreadBase {
        <<abstract>>
        #std::thread thread_
        #std::atomic~bool~ stop_
        +Start()
        +Stop()
        +Run()*
    }

    class Demuxer {
        -std::shared_ptr~Context~ ctx_
        +Open()
        +Close()
        +Seek(double incr, int seekByBytes)
        +Run()
        -DemuxLoop()
    }

    class Decoder {
        <<abstract>>
        #std::shared_ptr~Context~ ctx_
        #AVMediaType mediaType_
        #PacketQueue* queue_
        #int pktSerial_
        +Open()
        +Close()
        +Decode(AVCodecContext* codecCtx, AVFrame* frame)
        #ComponentOpen()*
        #ComponentClose()*
        +Run()*
    }

    class AudioDecoder {
        +ComponentOpen()
        +ComponentClose()
        +Run()
        -DecodeLoop()
        -EnqueueFrame(AVFrame* frame)
    }

    class VideoDecoder {
        +ComponentOpen()
        +Run()
        -DecodeLoop()
        -DropFrame(AVFrame* frame)
        -EnqueueFrame(AVFrame* frame)
    }

    class VideoPlayer {
        -std::shared_ptr~Context~ ctx_
        -SDL_Window* window_
        -SDL_Renderer* renderer_
        -SwsContext* swsCtx_
        -TTFRenderer ttfRenderer_
        +Open()
        +Start()
        +Close()
        +Run(int interval)
        -Refresh()
        -Display()
        -Render()
        -RenderLastTexture(Frame* vp)
        -ComputeTargetDelay()
        -UpdateVideoPts()
    }

    class TTFRenderer {
        -void* font_
        -SDL_Texture* overlayTexture_
        -int overlayW_
        -int overlayH_
        -std::string usrName_
        -std::string usrCompany_
        -std::string usrType_
        -bool dataDirty_
        +Init() int
        +Destroy()
        +SetOverlayData(name, company, type)
        +RenderOverlay(SDL_Renderer* renderer)
        -LoadFont()
        -BuildOverlayTexture(SDL_Renderer* renderer)
    }

    class AudioPlayer {
        -std::shared_ptr~Context~ ctx_
        -SDL_AudioDeviceID audioDevId_
        -std::atomic~int~ volume_
        -std::atomic~bool~ muted_
        +Open()
        +Start()
        +Stop()
        +Close()
        +ToggleMute()
        +UpdateVolume(int volume)
        -GetAudioData()
    }

    class Player {
        -std::shared_ptr~Context~ ctx_
        -std::shared_ptr~Demuxer~ demuxer_
        -std::shared_ptr~Decoder~ audioDecoder_
        -std::shared_ptr~Decoder~ videoDecoder_
        -std::shared_ptr~Decoder~ subtitleDecoder_
        -std::shared_ptr~AudioPlayer~ audioPlayer_
        -std::shared_ptr~VideoPlayer~ videoPlayer_
        +Open(const char* filename)
        +Start()
        +Close()
        +TogglePause()
        +ToggleMute()
        +ToggleFullScreen()
        +SeekForward(double incr)
        +SeekBackward(double incr)
        +IsPaused() bool
    }

    class Context {
        -const char* filename_
        -AVFormatContext* fmtCtx_
        -int audioIndex_
        -int videoIndex_
        -int subtitleIndex_
        -PacketQueue audioPacketQueue_
        -PacketQueue videoPacketQueue_
        -PacketQueue subtitlePacketQueue_
        -FrameQueue audioFrameQueue_
        -FrameQueue videoFrameQueue_
        -FrameQueue subtitleFrameQueue_
        -Clock audioClock_
        -Clock videoClock_
        -Clock* masterClock_
        -std::atomic~bool~ paused_
        -std::atomic~bool~ stop_
        -std::atomic~bool~ seekReq_
        -int64_t seekPos_
        -int64_t seekRel_
        -int seekFlags_
        -bool hasCustomData_
        -std::string usrName_
        -std::string usrCompany_
        -std::string usrType_
    }

    class PacketQueue {
        -std::queue~Packet~ queue_
        -int serial_
        -int64_t duration_
        +Put(AVPacket* pkt)
        +PutFlushPacket(int streamIndex)
        +Get(AVPacket* pkt, int block, int& serial)
        +Flush()
        +Serial() int
        +RequestAborted() bool
    }

    class FrameQueue {
        -Frame queue_[FRAME_QUEUE_SIZE]
        -PacketQueue* pktq_
        -int size_
        -int rindex_
        -int windex_
        +PeekWritable() Frame*
        +Push()
        +PeekReadable() Frame*
        +Next()
        +Peek() Frame*
        +PeekNext() Frame*
        +LastPos() int64_t
    }

    class Frame {
        -AVFrame* frame_
        -int serial_
        -double pts_
        -double duration_
        -int64_t pos_
    }

    class Clock {
        -double pts_
        -double ptsDrift_
        -double speed_
        -int* pktSerial_
        -SYNC_TYPE syncType_
        +Set(double pts, int serial)
        +Set_at(double pts, int serial, double time)
        +Get() double
        +Serial() int
    }

    class EventLoop {
        +Run(Player& player)
    }

    ThreadBase <|-- Demuxer : 继承
    ThreadBase <|-- Decoder : 继承
    Decoder <|-- AudioDecoder : 继承
    Decoder <|-- VideoDecoder : 继承

    Player *-- Demuxer : 组合
    Player *-- AudioDecoder : 组合
    Player *-- VideoDecoder : 组合
    Player *-- VideoPlayer : 组合
    Player *-- AudioPlayer : 组合
    VideoPlayer *-- TTFRenderer : 组合

    Demuxer --> Context : 依赖
    Decoder --> Context : 依赖
    VideoPlayer --> Context : 依赖
    AudioPlayer --> Context : 依赖
    TTFRenderer --> Context : 读取自定义数据

    Context *-- PacketQueue : 组合
    Context *-- FrameQueue : 组合
    Context *-- Clock : 组合

    FrameQueue --> PacketQueue : 依赖
    FrameQueue o-- Frame : 聚合
    Clock --> PacketQueue : 依赖(持有 serial_ 指针)

    EventLoop --> Player : 依赖
```

### 类图关系说明

| 关系符 | 含义 | 涉及类 |
|--------|------|--------|
| `<--` | 继承 | `ThreadBase` → `Demuxer`、`Decoder`；`Decoder` → `AudioDecoder`、`VideoDecoder` |
| `*--` | 组合（生命周期一致） | `Player` 持有各模块实例；`Context` 持有队列和时钟；`VideoPlayer` 持有 `TTFRenderer` |
| `o--` | 聚合（生命周期独立） | `FrameQueue` 聚合 `Frame`（环形缓冲区） |
| `-->` | 依赖（仅引用） | `Demuxer`/`Decoder`/`Player` 通过 `shared_ptr<Context>` 访问；`FrameQueue` 引用 `PacketQueue` 的 serial；`TTFRenderer` 经 `VideoPlayer` 间接读取 `Context` 的自定义数据字段 |


---

## Seek 流程

```mermaid
sequenceDiagram
    participant User as 用户
    participant Player as Player
    participant Demuxer as Demuxer
    participant PQ as PacketQueue

    User->>Player: 按快进/快退键
    Player->>Player: SeekForward(10s)
    Player->>Demuxer: Seek(incr, seekByBytes)
    Demuxer->>Demuxer: 计算目标时间 pos
    Note over Demuxer: 从 masterClock 获取当前时间<br/>NAN 时回退到 seekPos_
    Demuxer->>Demuxer: seekReq_ = true
    Demuxer->>Demuxer: notify_one()

    loop DemuxLoop 检测 seekReq_
        Demuxer->>Demuxer: avformat_seek_file(fmtCtx, -1, seekMin, seekTarget, seekMax, flags)
        alt seek 成功
            Demuxer->>PQ: Flush()
            PQ->>PQ: serial_++
            PQ->>PQ: 丢弃旧数据包
        end
        Demuxer->>Demuxer: seekReq_ = false
    end

    Note over PQ: serial 递增后<br/>解码器/渲染器自动丢弃旧帧
```

---

## 暂停/恢复流程

```mermaid
sequenceDiagram
    participant User as 用户
    participant Player as Player
    participant Demuxer as Demuxer (解复用线程)
    participant AudioDec as AudioDecoder (音频解码线程)
    participant VideoDec as VideoDecoder (视频解码线程)
    participant AudioPlayer as AudioPlayer (SDL音频回调)
    participant VideoPlayer as VideoPlayer (渲染线程)
    participant Clock as Clock (时钟)

    rect rgb(255, 235, 238)
        Note over User,VideoPlayer: ========== 暂停 ==========
        User->>Player: 按暂停键
        Player->>Player: TogglePause()
        Player->>Player: ctx_->paused_ = true

        Note over Demuxer: DemuxLoop 检测到 paused_ 变化
        Demuxer->>Demuxer: av_read_pause(fmtCtx)
        Demuxer->>Demuxer: pauseCond_.wait_for(40ms) + continue
        Note over Demuxer: 停止 av_read_frame()<br/>不再向 PacketQueue 投递新包

        Note over AudioDec: PacketQueue 为空<br/>queue_->Get() 阻塞等待<br/>解码线程自动挂起
        Note over VideoDec: 同理，PacketQueue 为空<br/>解码线程自动挂起

        AudioPlayer->>AudioPlayer: SDL 音频回调触发
        AudioPlayer->>AudioPlayer: GetAudioData() 检测 paused_
        Note over AudioPlayer: 返回 -1，不取帧<br/>填充静音数据 memset(0)
        AudioPlayer->>Clock: Set_at(旧PTS, serial, 当前时间)
        Note over Clock: pts_ 不变，lastUpdated_ 刷新<br/>Get() 返回值近似冻结

        VideoPlayer->>VideoPlayer: SDL 定时器触发 Refresh()
        VideoPlayer->>VideoPlayer: 检测 paused_ → goto display
        Note over VideoPlayer: 跳过帧推进/延迟计算/PTS更新<br/>仅 Display() 重绘当前帧
    end

    rect rgb(232, 245, 233)
        Note over User,VideoPlayer: ========== 恢复 ==========
        User->>Player: 按播放键
        Player->>Player: TogglePause()
        Player->>Player: ctx_->paused_ = false

        Note over Demuxer: DemuxLoop 检测到 paused_ 变化
        Demuxer->>Demuxer: av_read_play(fmtCtx)
        Demuxer->>Demuxer: 恢复 av_read_frame()
        Demuxer-->>AudioDec: 向 PacketQueue 投递音频包
        Demuxer-->>VideoDec: 向 PacketQueue 投递视频包

        Note over AudioDec: queue_->Get() 被唤醒<br/>恢复解码，帧入 FrameQueue
        Note over VideoDec: queue_->Get() 被唤醒<br/>恢复解码，帧入 FrameQueue

        AudioPlayer->>AudioPlayer: SDL 音频回调触发
        AudioPlayer->>AudioPlayer: GetAudioData() 检测 !paused_
        AudioPlayer->>AudioPlayer: 从 FrameQueue 取帧，重采样播放
        AudioPlayer->>Clock: Set_at(新PTS, serial, 当前时间)
        Note over Clock: pts_ 更新为新帧时间<br/>时钟恢复正常走时

        VideoPlayer->>VideoPlayer: Refresh() 检测 !paused_
        VideoPlayer->>VideoPlayer: 计算延迟 ComputeTargetDelay()
        VideoPlayer->>VideoPlayer: 帧推进 videoFrameQueue_.Next()
        VideoPlayer->>VideoPlayer: UpdateVideoPts(pts, serial)
        VideoPlayer->>VideoPlayer: Display() 渲染新帧
    end
```

# AVPlayer 主流程架构图

> 主要数据流与线程模型

```mermaid
flowchart TB
    classDef thread fill:#e1f5fe,stroke:#01579b,stroke-width:2px,color:#000
    classDef queue fill:#fff3e0,stroke:#e65100,stroke-width:2px,color:#000
    classDef io fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px,color:#000
    classDef sync fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000

    %% ===== Main Thread =====
    subgraph Main["🧵 main 线程"]
        M_Init["SDL_Init + Player::open/start
创建 Window/Renderer，启动 3 个子线程"]:::thread
        M_Loop["SDL 事件循环
事件驱动 + 20ms 超时 → 调用 refresh()"]:::thread
        M_Render["refresh → render
sws_scale 适配尺寸
SDL_RenderCopy（letterbox 居中）"]:::io
    end

    %% ===== Demuxer Thread =====
    subgraph DemuxerT["🧵 demuxer 线程"]
        D_Read["av_read_frame 读取文件"]:::thread
        D_Dispatch["按 stream_index 分发
AVPacket → video/audio packet queue"]:::thread
    end

    %% ===== Audio Decoder Thread =====
    subgraph AudioDecT["🧵 audio_decoder 线程"]
        AD_Decode["audio_packet_queue 取 packet
avcodec_send/receive → AVFrame"]:::thread
        AD_Enqueue["写入 audio_frame_queue"]:::thread
    end

    %% ===== Video Decoder Thread =====
    subgraph VideoDecT["🧵 video_decoder 线程"]
        VD_Decode["video_packet_queue 取 packet
avcodec_send/receive → AVFrame"]:::thread
        VD_Enqueue["写入 video_frame_queue"]:::thread
    end

    %% ===== SDL Audio Callback Thread =====
    subgraph SDLAudio["🧵 SDL Audio 回调线程"]
        S_CB["callback → get_audio_data
audio_frame_queue 取 frame
swr_convert 重采样 → SDL stream"]:::thread
    end

    %% ===== Queues =====
    VPQ["video_packet_queue"]:::queue
    APQ["audio_packet_queue"]:::queue
    VFQ["video_frame_queue"]:::queue
    AFQ["audio_frame_queue"]:::queue

    %% ===== Sync =====
    subgraph Sync["⏱️ A/V 同步"]
        Clock["master_clock（默认 audio_clock）"]:::sync
    end

    %% ===== 启动 ======
    M_Init -->|"启动"| D_Read
    M_Init -->|"启动"| AD_Decode
    M_Init -->|"启动"| VD_Decode

    %% ===== Demuxer 数据流 ======
    D_Read --> D_Dispatch
    D_Dispatch --> VPQ
    D_Dispatch --> APQ

    %% ===== Decoder 数据流 ======
    VPQ --> VD_Decode
    VD_Decode --> VD_Enqueue
    VD_Enqueue --> VFQ

    APQ --> AD_Decode
    AD_Decode --> AD_Enqueue
    AD_Enqueue --> AFQ

    %% ===== Audio 输出 ======
    AFQ --> S_CB

    %% ===== Video 输出 ======
    VFQ --> M_Render
    M_Loop --> M_Render

    %% ===== 同步 ======
    S_CB -.->|"pts 写入"| Clock
    Clock -.->|"参考调整 delay"| M_Render
```

***

# 音视频同步时序图

> 以音频时钟为 master clock，视频追音频。关键流程。

```mermaid
flowchart TB
    classDef audio fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#000
    classDef video fill:#fff3e0,stroke:#e65100,stroke-width:2px,color:#000
    classDef sync fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000
    classDef decision fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px,color:#000
    classDef output fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#000

    subgraph Audio["🧵 SDL Audio 回调线程"]
        A1["取帧 → audio_clock.set(pts)"]:::audio
        A2["master_clock = audio_clock"]:::sync
        A1 --> A2
    end

    subgraph Video["🧵 main 线程 VideoPlayer::refresh()"]
        V1["取帧 vp，计算 last_duration"]:::video
        V2{"compute_target_delay
diff = video_clock - master_clock
调整 delay"}:::decision
        V3{"time < frame_timer + delay?"}:::decision
        V3W["未到时间，等待"]:::video
        V4["frame_timer += delay"]:::video
        V5{"漂移修正?"}:::decision
        V5F["frame_timer = time"]:::video
        V6["update_video_pts(vp.pts)"]:::video
        V7{"next frame 已超时?"}:::decision
        V7D["丢帧 goto retry"]:::video
        V8["queue.next() + force_refresh"]:::output
    end

    V1 --> V2 --> V3
    V3 -->|"否"| V4
    V3 -->|"是"| V3W
    V4 --> V5
    V5 -->|"是"| V5F
    V5 -->|"否"| V6
    V5F --> V6
    V6 --> V7
    V7 -->|"是"| V7D
    V7 -->|"否"| V8

    V6 -.->|"video_clock 更新"| V2
    A2 -.->|"主时钟基准"| V2
```

## 关键代码位置

| 步骤                 | 类 + 函数                              |
| ------------------ | ----------------------------------- |
| 音频写主时钟             | `AudioPlayer::get_audio_data`       |
| A/V 同步调整 delay     | `VideoPlayer::compute_target_delay` |
| 判断显示时机 + 漂移修正 + 丢帧 | `VideoPlayer::refresh`              |
| 视频时钟写入             | `VideoPlayer::update_video_pts`     |

***

# 暂停与恢复流程图

> 暂停时音频设备不停，输出静音；时钟冻结；解封装阻塞；视频渲染保持当前帧。
> 恢复时各线程同步唤醒，时钟恢复递增。

```mermaid
flowchart TB
    classDef trigger fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef demux fill:#e1f5fe,stroke:#01579b,stroke-width:2px,color:#000
    classDef audio fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#000
    classDef video fill:#fff3e0,stroke:#e65100,stroke-width:2px,color:#000
    classDef clock fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000
    classDef action fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#000

    %% ===== 触发 =====
    subgraph Trigger["🎮 用户操作"]
        T_Pause["按空格 → Player::toggle_pause()"]:::trigger
        T_Resume["按空格 → Player::toggle_pause()"]:::trigger
    end

    %% ===== 暂停分支 =====
    subgraph Pause["⏸️ 暂停流程 (paused = true)"]
        direction TB
        P_Core["m_ctx->paused = true"]:::action

        subgraph P_Demux["demuxer 线程"]
            P_D1["检测 paused 变化"]:::demux
            P_D2["av_read_pause(fmt_ctx)
        停止从文件读取"]:::demux
            P_D3["m_pause_cond.wait_for(40ms)
        阻塞，不读新 packet"]:::demux
        end

        subgraph P_Audio["SDL Audio 回调线程"]
            P_A1["get_audio_data 检查 paused"]:::audio
            P_A2["return -1（不解码新帧）"]:::audio
            P_A3["m_current_audio_buf_data = nullptr"]:::audio
            P_A4["memset(stream, 0, len)
        输出静音"]:::audio
            P_A5["audio_clock 不更新
        （m_current_audio_clock 为 NAN）"]:::clock
        end

        subgraph P_Video["main 线程 VideoPlayer::refresh"]
            P_V1["检测 paused"]:::video
            P_V2["goto display
        跳过帧同步逻辑"]:::video
            P_V3["force_refresh 持续渲染当前帧
        （不切换到下一帧）"]:::video
        end
    end

    %% ===== 恢复分支 =====
    subgraph Resume["▶️ 恢复流程 (paused = false)"]
        direction TB
        R_Core["m_ctx->paused = false"]:::action

        subgraph R_Demux["demuxer 线程"]
            R_D1["wait_for 返回"]:::demux
            R_D2["av_read_play(fmt_ctx)
        恢复文件读取"]:::demux
            R_D3["av_read_frame → packet queue"]:::demux
        end

        subgraph R_Audio["SDL Audio 回调线程"]
            R_A1["get_audio_data 检查 !paused"]:::audio
            R_A2["从 audio_frame_queue 取帧"]:::audio
            R_A3["swr_convert 重采样"]:::audio
            R_A4["正常音频数据写入 stream"]:::audio
            R_A5["audio_clock.set_at(pts)
        主时钟恢复递增"]:::clock
        end

        subgraph R_Video["main 线程 VideoPlayer::refresh"]
            R_V1["不再 goto display"]:::video
            R_V2["compute_target_delay(diff)
        基于恢复的主时钟重新同步"]:::video
            R_V3["正常推进帧 → queue.next()"]:::video
        end
    end

    %% ===== 暂停连线 =====
    T_Pause --> P_Core
    P_Core --> P_D1 --> P_D2 --> P_D3
    P_Core --> P_A1 --> P_A2 --> P_A3 --> P_A4 --> P_A5
    P_Core --> P_V1 --> P_V2 --> P_V3

    %% ===== 恢复连线 =====
    T_Resume --> R_Core
    R_Core --> R_D1 --> R_D2 --> R_D3
    R_Core --> R_A1 --> R_A2 --> R_A3 --> R_A4 --> R_A5
    R_Core --> R_V1 --> R_V2 --> R_V3
```

## 关键设计点

| 层               | 暂停时                           | 恢复时                           |
| --------------- | ----------------------------- | ----------------------------- |
| **Demuxer**     | `av_read_pause` + 条件变量阻塞 40ms | `av_read_play` + 唤醒继续读 packet |
| **Audio 设备**    | **不停**，持续回调                   | 正常输出音频数据                      |
| **Audio 输出**    | `memset(stream, 0)` 静音        | 从帧队列取数据重采样输出                  |
| **Audio Clock** | 不更新，返回固定 `m_pts`              | `set_at(pts)` 恢复递增            |
| **Video 渲染**    | `goto display`，渲染当前帧不切换       | 正常走同步逻辑，推进帧                   |

## 关键代码位置

| 步骤      | 类 + 函数                                             |
| ------- | -------------------------------------------------- |
| 暂停状态切换  | `Player::toggle_pause`                             |
| 解封装暂停   | `Demuxer::demux_loop`                              |
| 音频静音输出  | `AudioPlayer::run` + `AudioPlayer::get_audio_data` |
| 视频保持当前帧 | `VideoPlayer::refresh`                             |
| 时钟冻结    | `Clock::get`                                       |

***

# Seek 跳转流程图

> 基于 serial 机制，确保 seek 后旧数据（packet / decoder 缓存 / frame）被正确丢弃。
> 涉及主线程（请求）、demuxer 线程（执行）、解码线程（flush）、播放线程（过滤旧帧）四个阶段。

```mermaid
flowchart TB
    classDef trigger fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef demux fill:#e1f5fe,stroke:#01579b,stroke-width:2px,color:#000
    classDef queue fill:#fff3e0,stroke:#e65100,stroke-width:2px,color:#000
    classDef decoder fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#000
    classDef player fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000
    classDef serial fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px,color:#000

    %% ===== 1. 请求 =====
    subgraph Req["🎮 主线程：用户按 ←/→"]
        R1["seek_forward(10s) / seek_backward(-10s)"]:::trigger
        R2["Demuxer::seek(incr)
        pos = master_clock.get() + incr
        clamp → [start_time, duration]"]:::demux
        R3["seek_pos = pos
        seek_rel = incr
        seek_req = 1
        notify_one()"]:::demux
        R1 --> R2 --> R3
    end

    %% ===== 2. 执行 =====
    subgraph Exec["🧵 demuxer 线程：demux_loop"]
        E1{"seek_req == 1?"}:::demux
        E2["avformat_seek_file
        (seek_min, seek_target, seek_max)"]:::demux
        E3{"ret >= 0?"}:::demux
        E4["flush 三个 packet queue
        audio / video / subtitle"]:::queue
        E5["serial++
        旧 packet 全部清空"]:::serial
        E6["seek_req = false"]:::demux
        E1 -->|"是"| E2 --> E3
        E3 -->|"成功"| E4 --> E5 --> E6
        E3 -->|"失败"| E6
    end

    %% ===== 3. 解码器 flush =====
    subgraph Dec["🧵 解码线程：Decoder::decode"]
        D1["从 packet_queue 取 packet"]:::decoder
        D2{"old_serial != m_pkt_serial?"}:::serial
        D3["avcodec_flush_buffers(codec_ctx)
        清空解码器内部缓存"]:::decoder
        D4{"queue.serial() != m_pkt_serial?"}:::serial
        D5["av_packet_unref(pkt)
        丢弃旧 serial packet"]:::decoder
        D6["avcodec_send/receive
        正常解码新 serial packet"]:::decoder
        D7["AVFrame → frame_queue
        （帧带新 serial）"]:::decoder
        D1 --> D2
        D2 -->|"是"| D3 --> D4
        D2 -->|"否"| D4
        D4 -->|"是（旧）"| D5 --> D1
        D4 -->|"否（新）"| D6 --> D7
    end

    %% ===== 4. 播放端过滤 =====
    subgraph Play["🧵 播放线程：过滤旧帧"]
        P_A["AudioPlayer::get_audio_data
        peek_readable → next
        while frame.serial != queue.serial()"]:::player
        P_V["VideoPlayer::refresh
        peek → next
        跳过旧 serial 的 frame"]:::player
        P_C["audio_clock.set(pts)
        video_clock 更新
        主时钟恢复"]:::player
        P_A --> P_C
        P_V --> P_C
    end

    %% ===== 阶段连接 =====
    R3 -->|"唤醒"| E1
    E5 -->|"serial 变化"| D2
    D7 -->|"新帧入队"| P_A
    D7 -->|"新帧入队"| P_V
```

## serial 机制详解

> serial 是 seek 正确性的核心保障，贯穿 packet queue → decoder → frame queue → 播放端全链路。

```mermaid
flowchart LR
    classDef before fill:#ffcdd2,stroke:#b71c1c,stroke-width:2px,color:#000
    classDef during fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef after fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px,color:#000
    classDef arrow fill:none,stroke:#616161,stroke-width:2px,color:#000

    subgraph Before["flush 前"]
        B_PQ["packet_queue
        serial=N
        （旧数据）"]:::before
        B_Dec["decoder
        serial=N
        （旧缓存）"]:::before
        B_FQ["frame_queue
        serial=N
        （旧帧）"]:::before
    end

    subgraph During["flush 时"]
        D_PQ["packet_queue
        清空所有 packet"]:::during
        D_Dec["decoder
        检测 serial 变化
        → avcodec_flush_buffers"]:::during
        D_FQ["frame_queue
        旧帧残留
        被 serial 比对跳过"]:::during
    end

    subgraph After["flush 后"]
        A_PQ["packet_queue
        serial=N+1
        （新数据）"]:::after
        A_Dec["decoder
        serial=N+1
        正常解码新 packet"]:::after
        A_FQ["frame_queue
        serial=N+1
        （新帧）"]:::after
    end

    B_PQ -->|"serial++"| D_PQ -->|"av_read_frame"| A_PQ
    B_Dec --> D_Dec --> A_Dec
    B_FQ --> D_FQ --> A_FQ

    A_PQ -.->|"send_packet"| A_Dec
    A_Dec -.->|"enqueue"| A_FQ
```

## 关键代码位置

| 步骤                       | 类 + 函数                        |
| ------------------------ | ----------------------------- |
| 计算目标位置 + 设置请求            | `Demuxer::seek`               |
| 执行 seek + flush queue    | `Demuxer::demux_loop`         |
| 清空队列 + serial 递增         | `PacketQueue::flush`          |
| 检测 serial 变化 + flush 解码器 | `Decoder::decode`             |
| 音频跳过旧帧                   | `AudioPlayer::get_audio_data` |
| 视频跳过旧帧                   | `VideoPlayer::refresh`        |

***

# 自定义数据 + TTF 叠加渲染设计

> 从 MP4 metadata 读取自定义数据(JSON / Protobuf 可切换),通过 SDL_ttf 渲染成半透明叠加层,每帧绘制到视频画面上。

## 整体数据流

```mermaid
flowchart LR
    classDef write fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef file fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px,color:#000
    classDef parse fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#000
    classDef ctx fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px,color:#000
    classDef render fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000
    classDef sdl fill:#ffe0b2,stroke:#e65100,stroke-width:2px,color:#000

    subgraph Write["写入端(离线工具)"]
        W1["json_data.h / proto_data.h
构造 JSON / Usr 消息"]:::write
        W2["av_dict_set(metadata,
key, value)"]:::write
        W3["copyMuxWithMetadata
重新封装 MP4"]:::write
        W1 --> W2 --> W3
    end

    MP4[("MP4 文件
metadata: video_custom_data
        video_custom_pb_data")]:::file

    subgraph Read["读取端(Player::Open)"]
        R1["av_dict_get(fmtCtx->metadata,
key, nullptr, 0)"]:::parse
        R2{"CUSTOM_DATA_FORMAT
宏分发"}:::parse
        R3J["ParseCustomDataFromJson
json::parse"]:::parse
        R3P["ParseCustomDataFromProtobuf
Usr::ParseFromString"]:::parse
        R4["CustomData {
  usrName, usrCompany, usrType }"]:::parse
        R1 --> R2
        R2 -->|"= 1"| R3J --> R4
        R2 -->|"= 2"| R3P --> R4
    end

    subgraph Store["Context 共享"]
        C1["hasCustomData_ = true
usrName_ / usrCompany_ / usrType_"]:::ctx
    end

    subgraph Display["VideoPlayer 渲染"]
        D1["Open: SetOverlayData
传自定义数据给 TTFRenderer"]:::render
        D2["每帧 Display:
Render() 视频帧
RenderOverlay() 叠加层"]:::render
        D3["SDL_RenderPresent"]:::sdl
        D1 --> D2 --> D3
    end

    W3 --> MP4
    MP4 --> R1
    R4 --> C1
    C1 --> D1
```

## 格式切换机制

```mermaid
flowchart TB
    classDef macro fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef branch fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#000
    classDef impl fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px,color:#000
    classDef side fill:#fce4ec,stroke:#880e4f,stroke-width:2px,color:#000

    M["custom_data.h
#define CUSTOM_DATA_FORMAT 1  // 1=JSON  2=Protobuf"]:::macro

    M -->|"编译期分支"| B{"#if
CUSTOM_DATA_FORMAT"}:::branch

    B -->|"== 1"| J["inline ParseCustomData()
→ ParseCustomDataFromJson()"]:::impl
    B -->|"== 2"| P["inline ParseCustomData()
→ ParseCustomDataFromProtobuf()"]:::impl

    J --> JK["metadata key:
video_custom_data"]:::side
    J --> JM["DEFAULT_MEDIA_PATH:
walking-dead-json.mp4"]:::side

    P --> PK["metadata key:
video_custom_pb_data"]:::side
    P --> PM["DEFAULT_MEDIA_PATH:
walking-dead-protobuf.mp4"]:::side
```

### 切换规则

| `CUSTOM_DATA_FORMAT` | 解析函数 | metadata key | 默认媒体文件 | 字段来源 |
|---|---|---|---|---|
| `1` (JSON) | `ParseCustomDataFromJson` | `video_custom_data` | `walking-dead-json.mp4` | `json::parse().value("usr_name", "")` 等 |
| `2` (Protobuf) | `ParseCustomDataFromProtobuf` | `video_custom_pb_data` | `walking-dead-protobuf.mp4` | `usr.name()` / `usr.company()` / `usr.type()` |

### 设计要点

- **统一入口**:`ParseCustomData()` 是 `inline` 函数,由宏在编译期决定调用哪个实现,运行期零开销
- **宏联动**:`DEFAULT_MEDIA_PATH` 与解析函数同步切换,避免"切了格式但还播放旧文件"的问题
- **降级安全**:两种解析失败时都返回 `hasData = false`,`TTFRenderer` 会跳过叠加层渲染,不影响播放
- **写入端独立**:`json_data.h` / `proto_data.h` 是离线写入工具,与播放器解耦,只要 metadata key 对得上就能读取

## 字体加载策略

```mermaid
flowchart TB
    classDef try fill:#fff9c4,stroke:#f57f17,stroke-width:2px,color:#000
    classDef ok fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px,color:#000
    classDef fail fill:#ffcdd2,stroke:#b71c1c,stroke-width:2px,color:#000

    Start["TTFRenderer::Init()
TTF_Init()"]:::try

    Start --> F1{"TTF_OpenFont(
assets/fonts/simhei.ttf)"}:::try
    F1 -->|"成功"| OK["font_ = f
log: TTF font loaded"]:::ok
    F1 -->|"失败"| F2{"项目根目录
simhei.ttf"}:::try
    F2 -->|"成功"| OK
    F2 -->|"失败"| F3{"C:\Windows\Fonts\
simhei.ttf"}:::try
    F3 -->|"成功"| OK
    F3 -->|"失败"| F4{"C:\Windows\Fonts\
msyh.ttc"}:::try
    F4 -->|"成功"| OK
    F4 -->|"失败"| F5{"C:\Windows\Fonts\
arial.ttf"}:::try
    F5 -->|"成功"| OK
    F5 -->|"失败"| WARN["font_ = nullptr
log: TTF_OpenFont failed
RenderOverlay 静默跳过"]:::fail

    OK --> Done["24px 字体加载完成
可渲染叠加层"]:::ok
    WARN --> Skip["播放器正常运行
但不显示叠加层"]:::fail
```

### Fallback 设计原则

| 优先级 | 路径 | 适用场景 |
|---|---|---|
| 1 | `assets/fonts/simhei.ttf` | 项目内打包,跨机器可复现(推荐) |
| 2 | 项目根目录 `simhei.ttf` | 开发期临时放置 |
| 3 | `C:\Windows\Fonts\simhei.ttf` | Windows 系统字体 |
| 4 | `C:\Windows\Fonts\msyh.ttc` | 微软雅黑(中文兜底) |
| 5 | `C:\Windows\Fonts\arial.ttf` | 英文兜底(中文会显示方块) |

**字体大小**:固定 24px,通过 `TTF_OpenFont(path, 24)` 加载。

## Texture 缓存机制

> 叠加层文字内容在播放过程中通常不变(自定义数据在 Open 阶段一次性设置),为避免每帧重复 `TTF_RenderUTF8_Blended`(开销大),采用脏标志 + Texture 缓存。

```mermaid
sequenceDiagram
    participant VP as VideoPlayer
    participant TTF as TTFRenderer
    participant Font as TTF_Font
    participant Ren as SDL_Renderer
    participant Tex as overlayTexture_

    Note over VP,Tex: ========== 初始化阶段 ==========

    VP->>TTF: SetOverlayData(name, company, type)
    alt 内容与上次相同
        Note over TTF: 直接 return,不置 dirty
    else 内容变化
        TTF->>TTF: dataDirty_ = true
    end

    Note over VP,Tex: ========== 每帧渲染阶段 ==========

    loop 每次 Display()
        VP->>TTF: RenderOverlay(renderer)
        alt font_ == nullptr
            Note over TTF: return(静默跳过)
        else dataDirty_ == true
            TTF->>Font: TTF_RenderUTF8_Blended(line, color)
            Font-->>TTF: SDL_Surface*(每行)
            TTF->>TTF: SDL_CreateRGBSurface(canvas)
            TTF->>TTF: SDL_FillRect(半透明黑底)
            TTF->>TTF: SDL_BlitSurface(逐行贴到 canvas)
            TTF->>Ren: SDL_CreateTextureFromSurface(canvas)
            Ren-->>TTF: 新 overlayTexture_
            TTF->>Tex: SDL_DestroyTexture(旧)
            TTF->>TTF: overlayTexture_ = 新
            TTF->>TTF: dataDirty_ = false
        else dataDirty_ == false
            Note over TTF: 复用 overlayTexture_(零开销)
        end
        TTF->>Ren: SDL_RenderCopy(overlayTexture_, dst={10,10})
    end

    Note over VP,Tex: ========== 销毁阶段 ==========
    VP->>TTF: Destroy()
    TTF->>Tex: SDL_DestroyTexture
    TTF->>Font: TTF_CloseFont
    TTF->>TTF: TTF_Quit
```

### 叠加层视觉规格

| 属性 | 值 |
|---|---|
| 位置 | 左上角 `(10, 10)` |
| 字体大小 | 24px |
| 文字颜色 | 白色 `RGBA(255,255,255,255)` |
| 背景色 | 半透明黑 `RGBA(0,0,0,160)` |
| 混合模式 | `SDL_BLENDMODE_BLEND` |
| 内边距 | 6px |
| 行间距 | `TTF_FontHeight(f)` |
| 显示内容 | `Name: xxx` / `Company: xxx` / `Type: xxx`(空字段跳过) |

## 关键代码位置

| 步骤 | 文件 + 函数 |
|---|---|
| 写入 metadata(离线) | `CustomMetadata/json_data.h` / `proto_data.h` |
| 读取 metadata + 解析 | `CustomMetadata/custom_data.cpp: ParseCustomDataFromJson / ParseCustomDataFromProtobuf` |
| 宏分发入口 | `CustomMetadata/custom_data.h: ParseCustomData` |
| 写入 Context | `AVPlayer/src/player.cpp: Player::Open` |
| 传给 TTFRenderer | `AVPlayer/src/video_player.cpp: VideoPlayer::Open` |
| TTF 初始化 + 字体加载 | `AVPlayer/src/ttf_renderer.cpp: TTFRenderer::Init / LoadFont` |
| Texture 构建 | `AVPlayer/src/ttf_renderer.cpp: BuildOverlayTexture` |
| 每帧叠加渲染 | `AVPlayer/src/ttf_renderer.cpp: RenderOverlay` |
| 调用叠加渲染 | `AVPlayer/src/video_player.cpp: VideoPlayer::Display / RenderLastTexture` |
| 资源释放 | `AVPlayer/src/ttf_renderer.cpp: TTFRenderer::Destroy` |


