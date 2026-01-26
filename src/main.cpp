// main.cpp
#include "configmanager.h"
#include "ipcmanager.h"
#include "logger.h"
#include "mainwindow.h"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
    Logger::log("Application starting...");

    // Log arguments for debugging
    /*
    // for (int i = 0; i < argc; ++i) {
    //     Logger::log(QString("Arg[%1]: %2").arg(i).arg(argv[i]));
    // }
    */

    QApplication app(argc, argv);
    app.setApplicationName("whatsit");
    app.setOrganizationName("whatsit");
    app.setDesktopFileName("whatsit");

    // Single-instance check
    if (IpcManager::notifyExistingInstance()) {
        return 0;
    }

    ConfigManager config;
    config.load();

    MainWindow w;
    if (!config.startMinimizedInTray()) {
        w.show();
    }

    return app.exec();
}
