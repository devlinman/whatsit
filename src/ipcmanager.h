// ipcmanager.h
#pragma once

#include <QObject>
#include <QLocalServer>

class IpcManager : public QObject
{
    Q_OBJECT
public:
    explicit IpcManager(QObject *parent = nullptr);

    // Start IPC server (called by main instance)
    void start();

    // Client-side helper:
    // returns true if another instance was found and notified
    static bool notifyExistingInstance();

signals:
    void raiseRequested();
    void openUrlRequested(const QUrl &url);

private:
    QLocalServer server;
};
