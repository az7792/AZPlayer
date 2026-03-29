#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include <QObject>

class PowerManager : public QObject {
    Q_OBJECT
    // 禁止拷贝
    Q_DISABLE_COPY(PowerManager)

    explicit PowerManager(QObject *parent = nullptr);
    ~PowerManager();
public:

    static PowerManager &instance();

    // 控制是否保持唤醒
    void setKeepScreenOn(bool on);
};

#endif // POWERMANAGER_H
