#ifndef UTILS_H
#define UTILS_H
#include <QAudioFormat>
#include <atomic>
#include <limits>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

class DeviceStatus {
public:
    bool initialized() {
        return m_audioInitialized && m_videoInitialized;
    }

    bool audioInitialized() const {
        return m_audioInitialized;
    }
    void setAudioInitialized(bool newAudioInitialized) {
        m_audioInitialized = newAudioInitialized;
    }

    bool videoInitialized() const {
        return m_videoInitialized;
    }
    void setVideoInitialized(bool newVideoInitialized) {
        m_videoInitialized = newVideoInitialized;
    }

    static DeviceStatus &instance() {
        static DeviceStatus ds;
        return ds;
    }

private:
    bool m_audioInitialized, m_videoInitialized;

    DeviceStatus(const DeviceStatus &) = delete;
    DeviceStatus &operator=(const DeviceStatus &) = delete;

    DeviceStatus() : m_audioInitialized(false), m_videoInitialized(false) {}
};

struct AVPktItem {
    AVPacket *pkt = nullptr;
    int seekCnt = 0;
};

struct AVFrmItem {
    AVFrame *frm = nullptr;
    int seekCnt = 0;
    double pts = std::numeric_limits<double>::quiet_NaN();
};

/**
 * 单生产者单消费者的有限无锁队列(缓冲区)
 */
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : m_capacity(capacity + 1), // 多分配一个，用于处理满条件
          m_buffer(m_capacity) {}

    bool push(const T &value) {
        // 生产者本地快照
        size_t current_tail = m_tail.v.load(std::memory_order_relaxed);
        size_t next_tail = nextIndex(current_tail);

        // 检查队列是否已满
        if (next_tail == m_head.v.load(std::memory_order_acquire)) {
            return false; // 队列满，推送失败
        }

        // 将数据存入当前tail指向的位置
        m_buffer[current_tail] = value;

        // 发布tail的更新，确保前面的数据存储对消费者可见
        m_tail.v.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T &value) {
        // 消费者本地快照
        size_t current_head = m_head.v.load(std::memory_order_relaxed);

        // 检查队列是否为空
        if (current_head == m_tail.v.load(std::memory_order_acquire)) {
            return false; // 队列空，弹出失败
        }

        // 从当前head指向的位置取出数据
        value = m_buffer[current_head];

        // 发布head的更新，告知生产者新的头部位置
        m_head.v.store(nextIndex(current_head), std::memory_order_release);
        return true;
    }

    /**
     * 获取第一个有效的元素
     * @note 请确保pop与peekSecond只在同一个线程下使用
     */
    bool peekFirst(T &value) {
        // 消费者本地快照
        size_t current_head = m_head.v.load(std::memory_order_relaxed);

        // 检查队列是否为空
        if (current_head == m_tail.v.load(std::memory_order_acquire)) {
            return false; // 队列空，弹出失败
        }

        // 从当前head指向的位置取出数据
        value = m_buffer[current_head];
        return true;
    }

    /**
     * 获取第二个有效的元素
     * @note 请确保pop与peekSecond只在同一个线程下使用
     */
    bool peekSecond(T &value) {
        size_t current_head = m_head.v.load(std::memory_order_acquire);
        size_t current_tail = m_tail.v.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false;
        }

        size_t secondIdx = nextIndex(current_head);

        if (secondIdx == current_tail) {
            return false;
        }

        value = m_buffer[secondIdx];
        return true;
    }

    size_t size() const {
        size_t head = m_head.v.load(std::memory_order_acquire);
        size_t tail = m_tail.v.load(std::memory_order_acquire);
        if (tail >= head)
            return tail - head;
        else
            return m_capacity - head + tail;
    }

    size_t capacity() const {
        return m_capacity - 1;
    }

private:
    size_t nextIndex(size_t index) const {
        return (index + 1) % m_capacity;
    }

    const size_t m_capacity; // 环形缓冲区的总容量（包括浪费的一个位置）
    std::vector<T> m_buffer; // 数据缓冲区

    struct alignas(64) PaddedAtomic {
        std::atomic<size_t> v;
        char pad[64 - sizeof(std::atomic<size_t>)]; // 填充缓存行,防止伪共享
        PaddedAtomic() : v(0) {}
    };
    PaddedAtomic m_head; // 读索引（消费者使用）
    PaddedAtomic m_tail; // 写索引（生产者使用）
};

using sharedPktQueue = std::shared_ptr<SPSCQueue<AVPktItem>>;
using sharedFrmQueue = std::shared_ptr<SPSCQueue<AVFrmItem>>;

struct AudioPar {
    int sampleRate = 0;                               // 采样率
    AVSampleFormat sampleFormat = AV_SAMPLE_FMT_NONE; // 采样格式
    AVChannelLayout ch_layout;                        // 通道布局

    AudioPar() {
        av_channel_layout_default(&ch_layout, 0);
    }

    ~AudioPar() {
        av_channel_layout_uninit(&ch_layout);
    }

    void reset() {
        sampleRate = 0;
        sampleFormat = AV_SAMPLE_FMT_NONE;
        av_channel_layout_default(&ch_layout, 0);
    }
};

struct AVPacketGuard {
    AVPacket *p = nullptr;
    AVPacketGuard() : p(av_packet_alloc()) {}
    ~AVPacketGuard() {
        if (p)
            av_packet_free(&p);
    }

    void unref() {
        if (p)
            av_packet_unref(p);
    }

    // 禁止拷贝
    AVPacketGuard(const AVPacketGuard &) = delete;
    AVPacketGuard &operator=(const AVPacketGuard &) = delete;

    // 支持移动
    AVPacketGuard(AVPacketGuard &&other) noexcept : p(other.p) {
        other.p = nullptr;
    }
    AVPacketGuard &operator=(AVPacketGuard &&other) noexcept {
        if (this != &other) {
            if (p)
                av_packet_free(&p);
            p = other.p;
            other.p = nullptr;
        }
        return *this;
    }
};

struct AVFrameGuard {
    AVFrame *f = nullptr;
    AVFrameGuard() : f(av_frame_alloc()) {}
    ~AVFrameGuard() {
        if (f)
            av_frame_free(&f);
    }

    void unref() {
        if (f)
            av_frame_unref(f);
    }

    // 禁止拷贝
    AVFrameGuard(const AVFrameGuard &) = delete;
    AVFrameGuard &operator=(const AVFrameGuard &) = delete;

    // 支持移动
    AVFrameGuard(AVFrameGuard &&other) noexcept : f(other.f) {
        other.f = nullptr;
    }
    AVFrameGuard &operator=(AVFrameGuard &&other) noexcept {
        if (this != &other) {
            if (f)
                av_frame_free(&f);
            f = other.f;
            other.f = nullptr;
        }
        return *this;
    }
};

struct AVCodecContextGuard {
    AVCodecContext *c = nullptr;
    AVCodecContextGuard() = default;
    ~AVCodecContextGuard() {
        if (c)
            avcodec_free_context(&c);
    }

    // 禁止拷贝
    AVCodecContextGuard(const AVCodecContextGuard &) = delete;
    AVCodecContextGuard &operator=(const AVCodecContextGuard &) = delete;

    // 支持移动
    AVCodecContextGuard(AVCodecContextGuard &&other) noexcept : c(other.c) {
        other.c = nullptr;
    }
    AVCodecContextGuard &operator=(AVCodecContextGuard &&other) noexcept {
        if (this != &other) {
            if (c)
                avcodec_free_context(&c);
            c = other.c;
            other.c = nullptr;
        }
        return *this;
    }
};

#endif // UTILS_H
