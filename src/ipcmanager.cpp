// ipcmanager.cpp
#include "ipcmanager.h"
#include "logger.h"

#include <QLocalSocket>

static constexpr const char *IPC_NAME = "whatsit-ipc";

IpcManager::IpcManager(QObject *parent)
: QObject(parent)
{
}

bool IpcManager::notifyExistingInstance()
{
    Logger::log("Checking for existing instance...");
    QLocalSocket socket;
    socket.connectToServer(IPC_NAME);

    if (!socket.waitForConnected(100))
        return false; // No running instance

    Logger::log("Existing instance found. Sending 'raise' signal.");
    socket.write("raise");
    socket.flush();
    socket.waitForBytesWritten(100);
    socket.disconnectFromServer();

    return true;
}

void IpcManager::start()
{
    Logger::log("Starting IPC server...");
    // Clean up stale socket (crash-safe)
    QLocalServer::removeServer(IPC_NAME);

    server.listen(IPC_NAME);

    connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *s = server.nextPendingConnection();
        connect(s, &QLocalSocket::readyRead, this, [=] {
            if (s->readAll() == "raise") {
                Logger::log("IPC 'raise' signal received.");
                emit raiseRequested();
            }
            s->disconnectFromServer();
        });
    });
}
