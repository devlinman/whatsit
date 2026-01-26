// ipcmanager.cpp
#include "ipcmanager.h"
#include "logger.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QUrl>

static constexpr const char *IPC_NAME = "whatsit-ipc";

IpcManager::IpcManager(QObject *parent) : QObject(parent) {}

bool IpcManager::notifyExistingInstance() {
    Logger::log("Checking for existing instance...");

    QStringList args = QCoreApplication::arguments();
    // Debug: Log all arguments for debugging
    /*
    // for (const auto& arg : args) {
    //      Logger::log("Raw Arg: " + arg);
    // }
    */

    QLocalSocket socket;
    socket.connectToServer(IPC_NAME);

    if (!socket.waitForConnected(100))
        return false; // No running instance

    Logger::log("Existing instance found.");

    QString message = "raise";

    // Check for URL argument
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].startsWith("http") || args[i].startsWith("whatsapp")) {
            message += "|" + args[i];
            // Debug:
            // Logger::log("Found URL argument: " + args[i]);
            break;
        }
    }

    // Debug:
    // Logger::log("Sending IPC message: " + message);
    socket.write(message.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(100);
    socket.disconnectFromServer();

    return true;
}

void IpcManager::start() {
    Logger::log("Starting IPC server...");
    // Clean up stale socket (crash-safe)
    QLocalServer::removeServer(IPC_NAME);

    server.listen(IPC_NAME);

    connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *s = server.nextPendingConnection();
        connect(s, &QLocalSocket::readyRead, this, [=] {
            QByteArray data = s->readAll();
            QString message = QString::fromUtf8(data);
            // Debug:
            // Logger::log("IPC message received: " + message);

            QStringList parts = message.split('|'); // split using '|'
            if (!parts.isEmpty() && parts[0] == "raise") {
                emit raiseRequested();
                if (parts.size() > 1) {
                    QUrl url(parts[1]);
                    if (url.isValid()) {
                        // Logger::log("IPC URL request: " + url.toString());
                        emit openUrlRequested(url);
                    }
                }
            }
            s->disconnectFromServer();
        });
    });
}
