// main.cpp
#include "configmanager.h"
#include "ipcmanager.h"
#include "logger.h"
#include "mainwindow.h"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
    Logger::log("Application starting...");

    QApplication app(argc, argv);
    app.setApplicationName("whatsit");
    app.setOrganizationName("whatsit");
    app.setDesktopFileName("whatsit");

    QStringList args = app.arguments();
    bool showFlag = false;
    bool hideFlag = false;
    bool helpFlag = false;
    int flagCount = 0;

    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == "show") {
            showFlag = true;
            flagCount++;
        } else if (arg == "hide") {
            hideFlag = true;
            flagCount++;
        } else if (arg == "help" || arg == "--help" || arg == "-h") {
            helpFlag = true;
            flagCount++;
        }
    }

    if (flagCount > 1) {
        std::cerr << "Error: Only one of 'show', 'hide', or 'help' flags can be passed." << std::endl;
        std::cout << "Usage: whatsit [show|hide|help] [url]" << std::endl;
        return 1;
    }

    if (helpFlag) {
        std::cout << "Usage: whatsit [options] [url]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  show    Start the application with the window visible." << std::endl;
        std::cout << "  hide    Start the application minimized to the tray." << std::endl;
        std::cout << "  help    Show this help message." << std::endl;
        std::cout << std::endl;
        std::cout << "Arguments:" << std::endl;
        std::cout << "  url     Optional URL to open (starts with http, https, or whatsapp)." << std::endl;
        return 0;
    }

    // Single-instance check
    QString ipcCommand = "raise";
    if (hideFlag) {
        ipcCommand = "hide";
    }
    
    if (IpcManager::notifyExistingInstance(ipcCommand)) {
        return 0;
    }

    ConfigManager config;
    config.load();

    MainWindow w;
    
    bool startMinimized = config.startMinimizedInTray();
    
    if (showFlag) {
        startMinimized = false;
    } else if (hideFlag) {
        startMinimized = true;
    }

    if (!startMinimized) {
        w.show();
    }

    return app.exec();
}
