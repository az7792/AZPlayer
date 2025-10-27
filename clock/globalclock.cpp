// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalclock.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <qDebug>

namespace {
    constexpr double INVALID_DOUBLE = std::numeric_limits<double>::quiet_NaN();
}

double getRelativeSeconds() {
    auto nowTime = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(nowTime.time_since_epoch()).count();
}

Clock::Clock(ClockType type) : m_type(type) {
    setClock(INVALID_DOUBLE);
}

void Clock::setClock(double pts) {
    setClock(pts, getRelativeSeconds());
}

void Clock::setClock(double pts, double time) {
    m_updatePts = pts;
    m_updateTime = time;
}

ClockType Clock::type() {
    return m_type;
}

bool Clock::isvalidated() {
    return !std::isnan(m_updatePts);
}

double Clock::getPts() {
    if (m_paused) {
        return m_updatePts;
    } else {
        return m_updatePts + (getRelativeSeconds() - m_updateTime) * m_speed;
    }
}

void Clock::setPaused(bool newPaused) {
    m_paused = newPaused;
}

void Clock::setSpeed(double newSpeed) {
    setClock(getPts()); // 保证从时钟更新起点 到 当前时间点都是以这个速度运行的
    m_speed = newSpeed;
}

void Clock::syncToClock(Clock &clk) {
    double nowPts = getPts();
    double newPts = clk.getPts();
    if (!std::isnan(newPts) && (std::isnan(nowPts) || std::abs(newPts - nowPts) > 10.0)) {
        setClock(newPts);
    }
}

void Clock::togglePaused() {
    setClock(getPts());
    m_paused = !m_paused;
}

GlobalClock::GlobalClock()
    : m_audioClk(ClockType::AUDIO), m_videoClk(ClockType::VIDEO),
      m_externalClk(ClockType::EXTERNAL), m_maxFrameDuration(10.0) {
}

double GlobalClock::seekTs() const {
    return m_seekTs;
}

void GlobalClock::setSeekTs(double newSeekTs) {
    m_seekTs = newSeekTs;
}

int GlobalClock::seekCnt() const {
    return m_seekCnt;
}

void GlobalClock::addSeekCnt() {
    m_seekCnt++;
}

void GlobalClock::reset() {
    m_videoClk.setClock(INVALID_DOUBLE);
    m_audioClk.setClock(INVALID_DOUBLE);
    m_externalClk.setClock(INVALID_DOUBLE);
    m_videoClk.m_speed = m_audioClk.m_speed = m_externalClk.m_speed = 1.0;
    m_videoClk.m_paused = m_audioClk.m_paused = m_externalClk.m_paused = false;
    m_seekTs = 0.0;
}

GlobalClock &GlobalClock::instance() {
    static GlobalClock gl;
    return gl;
}

double GlobalClock::getMainPts() {
    switch (m_mainClockType) {
    case ClockType::AUDIO:
        return m_audioClk.getPts();
    case ClockType::VIDEO:
        return m_videoClk.getPts();
    case ClockType::EXTERNAL:
        return m_externalClk.getPts();
    default:
        return INVALID_DOUBLE;
    }
}

ClockType GlobalClock::mainClockType() {
    return m_mainClockType;
}

void GlobalClock::togglePaused() {
    m_videoClk.togglePaused();
    m_audioClk.togglePaused();
    m_externalClk.togglePaused();
}

double GlobalClock::audioPts() {
    return m_audioClk.getPts();
}

double GlobalClock::videoPts() {
    return m_videoClk.getPts();
}

double GlobalClock::externalPts() {
    return m_externalClk.getPts();
}

double GlobalClock::maxFrameDuration() {
    return m_maxFrameDuration;
}

void GlobalClock::setAudioClk(double pts) {
    setAudioClk(pts, getRelativeSeconds());
}
void GlobalClock::setAudioClk(double pts, double time) {
    m_audioClk.setClock(pts, time);
}

void GlobalClock::setVideoClk(double pts) {
    setVideoClk(pts, getRelativeSeconds());
}
void GlobalClock::setVideoClk(double pts, double time) {
    m_videoClk.setClock(pts, time);
}

void GlobalClock::setExternalClk(double pts) {
    setExternalClk(pts, getRelativeSeconds());
}
void GlobalClock::setExternalClk(double pts, double time) {
    m_externalClk.setClock(pts, time);
}

void GlobalClock::setMaxFrameDuration(double newMaxFrameDuration) {
    m_maxFrameDuration = newMaxFrameDuration;
}

void GlobalClock::setMainClockType(ClockType newMainClockType) {
    m_mainClockType = newMainClockType;
}

void GlobalClock::syncExternalClk(ClockType type) {
    if (type == ClockType::AUDIO)
        m_externalClk.syncToClock(m_audioClk);
    else if (type == ClockType::VIDEO)
        m_externalClk.syncToClock(m_videoClk);
}
