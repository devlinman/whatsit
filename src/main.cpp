// main.cpp
#include <QApplication>
#include <iostream>
#include "mainwindow.h"
#include "ipcmanager.h"
#include "logger.h"
#include "configmanager.h"

int main(int argc, char *argv[])
{
    Logger::log("Application starting...");
    QApplication app(argc, argv);
    app.setApplicationName("whatsit");
    app.setOrganizationName("whatsit");
    app.setDesktopFileName("whatsit");

    // Single-instance check
    if (IpcManager::notifyExistingInstance()) {
        return 0;
    }

    // Apply Dark Mode Preference
    ConfigManager config;
    config.load();

    // Get current flags (inherited from parent process if restarting)
    QByteArray flagsEnv = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    QString flags = QString::fromLocal8Bit(flagsEnv);

    // Clean up our specific flags to prevent accumulation/conflicts
    flags.remove("--blink-settings=preferredColorScheme=1");
    flags.remove("--blink-settings=preferredColorScheme=2");
    
    // Cleanup any double spaces created by removal
    flags = flags.simplified(); 

    if (config.preferDarkMode()) {
        if (!flags.isEmpty()) flags += " ";
        flags += "--blink-settings=preferredColorScheme=1";
    } else {
        if (!flags.isEmpty()) flags += " ";
        flags += "--blink-settings=preferredColorScheme=2";
    }

    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.toLocal8Bit());

    MainWindow w;
    if (!config.startMinimizedInTray()) {
        w.show();
    }

    return app.exec();
}
