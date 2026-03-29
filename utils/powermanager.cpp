#include "powermanager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

PowerManager::PowerManager(QObject *parent)
    : QObject{parent}
{}

PowerManager::~PowerManager() {
    setKeepScreenOn(false);
}

PowerManager &PowerManager::instance() {
    static PowerManager ins;
    return ins;
}

void PowerManager::setKeepScreenOn(bool on) {
#ifdef Q_OS_WIN
    if (on) {
        // ES_SYSTEM_REQUIRED: 阻止进入睡眠模式
        // ES_DISPLAY_REQUIRED: 阻止关闭显示器
        // ES_CONTINUOUS: 使设置持续生效直到下次调用
        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
#else
    Q_UNUSED(on);
#endif
}
