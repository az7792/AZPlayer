// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GLOBALCLOCK_H
#define GLOBALCLOCK_H

// 获取相对且单调的时间(秒)
[[nodiscard]] double getRelativeSeconds();

enum class ClockType {
    NONE = -1,
    AUDIO,
    VIDEO,
    EXTERNAL,
};

class GlobalClock;

class Clock {
public:
    Clock(ClockType type = ClockType::NONE);
    // 以当前pts作为时钟起始点，默认以调用时时间为起始的现实时间
    void setClock(double pts);

    // 以指定pts和time作为起始点
    void setClock(double pts, double time);

    // 获取时钟类型
    [[nodiscard]] ClockType type() const;

    // 时钟是否有效
    [[nodiscard]] bool isValidated() const;

    // 获取时钟当前这个时刻的pts(秒)
    [[nodiscard]] double getPts() const;

    void setPaused(bool newPaused);
    void setSpeed(double newSpeed);

    // 同步时间到其他时钟
    void syncToClock(Clock &clk);

    void togglePaused();

private:
    double m_updatePts;    // 更新时钟时的pts(秒)
    double m_updateTime;   // 更新时钟时的现实时间/系统时间(秒)
    bool m_paused = false; // 是否暂停
    double m_speed = 1.0;  // 现实流逝时间*m_speed = 当前时钟流逝的时间,从时钟更新起点 到 当前时间点都是以这个速度运行的
    ClockType m_type;
    friend GlobalClock;
};

class GlobalClock {
    GlobalClock(const GlobalClock &) = delete;
    GlobalClock &operator=(const GlobalClock &) = delete;
    GlobalClock(GlobalClock &&) = delete;
    GlobalClock &operator=(GlobalClock &&) = delete;

public:
    void reset();

    static GlobalClock &instance();
    [[nodiscard]] double getMainPts() const;
    [[nodiscard]] ClockType mainClockType() const;

    void togglePaused();

    [[nodiscard]] double audioPts() const;
    [[nodiscard]] double videoPts() const;
    [[nodiscard]] double externalPts() const;
    [[nodiscard]] double maxFrameDuration() const;

    void setAudioClk(double pts);
    void setAudioClk(double pts, double time);

    void setVideoClk(double pts);
    void setVideoClk(double pts, double time);

    void setExternalClk(double pts);
    void setExternalClk(double pts, double time);

    void setMaxFrameDuration(double newMaxFrameDuration);
    void setMainClockType(ClockType newMainClockType);

    // 将外部时钟同步到指定时钟
    void syncExternalClk(ClockType type);

private:
    GlobalClock();
    ~GlobalClock() = default;
    ClockType m_mainClockType = ClockType::AUDIO;
    Clock m_audioClk, m_videoClk, m_externalClk;
    double m_maxFrameDuration;
};

#endif // GLOBALCLOCK_H
