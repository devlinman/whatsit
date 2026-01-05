// mainwindow.cpp
#include "mainwindow.h"

#include <KIconDialog>
#include <KIconLoader>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <unistd.h>

#include "logger.h"

static constexpr int DEFAULT_W = 1200;
static constexpr int DEFAULT_H = 800;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), view(new QWebEngineView(this)), web(nullptr),
      tray(nullptr), ipc(nullptr) {
    Logger::log("MainWindow constructor");
    // Prevent Qt from quitting when last window is hidden
    qApp->setQuitOnLastWindowClosed(false);

    setCentralWidget(view);

    config.load();
    Logger::setFileLoggingEnabled(config.debugLoggingEnabled());

    if (config.rememberWindowSize())
        resize(config.windowSize());
    else
        resize(DEFAULT_W, DEFAULT_H);

    if (config.maximizedByDefault()) {
        if (!config.startMinimizedInTray())
            showMaximized();
        else
            setWindowState(Qt::WindowMaximized);
    }

    web = new WebEngineHelper(view, &config, this);
    web->initialize();

    // Set initial zoom level
    view->setZoomFactor(config.zoomLevel());

    tray = new TrayManager(this);
    tray->initialize();

    QString trayIconToUse = "whatsit";
    QString appIconToUse = "whatsit";

    QString customTrayIconStr = config.customTrayIcon();
    QString customAppIconStr = config.customAppIcon();

    if (!customTrayIconStr.isEmpty()) {
        trayIconToUse = customTrayIconStr;
    }

    if (!customAppIconStr.isEmpty()) {
        appIconToUse = customAppIconStr;
        Logger::log("Found Custom App icon:");
        Logger::log(customAppIconStr);
        ensureDesktopFile(customAppIconStr);
    }

    QIcon icon = QIcon::fromTheme(trayIconToUse);
    if (icon.isNull())
        icon = QIcon(trayIconToUse);

    if (icon.isNull() && trayIconToUse != "whatsit") {
        trayIconToUse = "whatsit";
        icon = QIcon::fromTheme(trayIconToUse);
        if (icon.isNull())
            icon = QIcon(trayIconToUse);
    }
    if (icon.isNull()) {
        QMessageBox::critical(
            nullptr, QObject::tr("Fatal Error!"),
            QObject::tr(
                "No tray icon could be found.\n\n"
                "Please reinstall the application or ensure the icon theme "
                "is correctly installed."));

        qApp->quit();
        return;
    }

    tray->setIcon(trayIconToUse);

    connect(tray, &TrayManager::showRequested, this, &MainWindow::showAndRaise);
    connect(tray, &TrayManager::hideRequested, this, &QWidget::hide);
    connect(tray, &TrayManager::activated, this, [this] {
        if (isVisible() && isActiveWindow() && !isMinimized()) {
            hide();
        } else {
            showAndRaise();
        }
    });

    ipc = new IpcManager(this);
    connect(ipc, &IpcManager::raiseRequested, this, &MainWindow::showAndRaise);
    ipc->start();

    memoryTimer = new QTimer(this);
    connect(memoryTimer, &QTimer::timeout, this, &MainWindow::checkMemoryUsage);
    if (config.memoryLimit() > 0) {
        memoryTimer->start(30000); // Check every 30 seconds
    }

    auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
    quitShortcut->setContext(Qt::ApplicationShortcut);
    connect(quitShortcut, &QShortcut::activated, this,
            [this] { handleExitRequest(); });

    auto *fullQuitShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Q"), this);
    fullQuitShortcut->setContext(Qt::ApplicationShortcut);
    connect(fullQuitShortcut, &QShortcut::activated, this, [this] {
        Logger::log("Ctrl+Shift+Q pressed -> Force Quitting application.");
        qApp->quit();
    });

    setupMenus();

    // URL Logic
    QString targetUrlStr = config.customUrl();
    if (targetUrlStr.isEmpty()) {
        targetUrlStr = "https://web.whatsapp.com";
    }

    QUrl targetUrl(targetUrlStr);
    if (!targetUrl.isValid() || targetUrl.scheme().isEmpty()) {
        Logger::log("Invalid Custom URL: " + targetUrlStr +
                    " -> Fallback to Google.");
        targetUrl = QUrl("https://google.com");
    }

    Logger::log("Loading URL: " + targetUrl.toString());
    view->load(targetUrl);
}

MainWindow::~MainWindow() {
    if (config.rememberWindowSize())
        config.setWindowSize(size());

    config.sync();
}

// unified show / raise behavior
void MainWindow::showAndRaise() {
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);

    show();
    raise();
    activateWindow();
}

// SINGLE exit decision point
void MainWindow::handleExitRequest() {
    if (config.minimizeToTray()) {
        Logger::log("Minimizing to tray instead of quitting.");
        hide();
    } else {
        Logger::log("Quitting application.");
        qApp->quit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (config.minimizeToTray()) {
        Logger::log("Close event ignored -> Minimizing to tray.");
        hide();
        event->ignore();
    } else {
        Logger::log("Close event accepted -> Quitting.");
        event->accept();
        qApp->quit();
    }
}

void MainWindow::hideEvent(QHideEvent *event) {
    QMainWindow::hideEvent(event);
    updateMemoryState();
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    updateMemoryState();
}

void MainWindow::updateMemoryState() {
    if (!view)
        return;

    if (config.useLessMemory() && !isVisible()) {
        // If hidden and memory optimization is ON, unload the page
        if (view->url().toString() != "about:blank") {
            Logger::log("Use Less Memory: Unloading content to about:blank");
            view->stop(); // Stop any pending loads
            view->setUrl(QUrl("about:blank"));
        }
    } else {
        // If visible OR memory optimization is OFF, ensure we have the correct
        // page
        if (view->url().toString() == "about:blank") {
            Logger::log("Use Less Memory: Restoring content");
            QString target = config.customUrl();
            if (target.isEmpty())
                target = "https://web.whatsapp.com";
            view->setUrl(QUrl(target));
        }
    }
}

void MainWindow::checkMemoryUsage() {
    int limitGb = config.memoryLimit();
    if (limitGb <= 0)
        return;

    qint64 totalRssKb = 0;
    pid_t pgid = getpgrp();

    QProcess ps;
    ps.start("ps", {"-o", "rss=", "-g", QString::number(pgid)});
    if (ps.waitForFinished()) {
        QString output = ps.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            totalRssKb += line.trimmed().toLongLong();
        }
    }

    double totalGb = totalRssKb / (1024.0 * 1024.0);
    if (totalGb > limitGb) {
        Logger::log(QString("MEMORY KILL SWITCH TRIGGERED: %1 GB used, limit "
                            "is %2 GB. Quitting.")
                        .arg(totalGb, 0, 'f', 2)
                        .arg(limitGb));
        qApp->quit();
    }
}

void MainWindow::ensureDesktopFile(const QString &iconPath) {
    if (iconPath.isEmpty())
        return;

    QString localShareAppsDir =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);

    QDir appsDir(localShareAppsDir);
    appsDir.mkpath(".");

    QString localDesktopFile = appsDir.filePath("whatsit.desktop");
    QFile desktopFile(localDesktopFile);

    bool needToUpdate = true;

    if (desktopFile.exists()) {
        if (desktopFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = desktopFile.readAll();
            desktopFile.close();
            // Check if existing file has the same icon
            if (content.contains("Icon=" + iconPath + "\n")) {
                needToUpdate = false;
            }
        }
    }

    if (needToUpdate) {
        if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream out(&desktopFile);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Version=1.0\n";
            out << "Name=whatsit\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=" << iconPath << "\n";
            out << "Terminal=false\n";
            out << "Categories=Utility;\n";
            // StartupWMClass ensures the window manager groups the window
            // correctly with this desktop file
            out << "StartupWMClass=whatsit\n";
            desktopFile.close();
            Logger::log("Updated desktop file with custom icon: " + iconPath);

            // Safely rebuild KDE system config cache
            rebuildKCache();

        } else {
            Logger::log("Failed to write desktop file: " + localDesktopFile);
        }
    } else {
        Logger::log("Desktop file is up to date.");
    }
}

void MainWindow::rebuildKCache() {
    const QString kbuild = QStandardPaths::findExecutable("kbuildsyscoca6");
    if (!kbuild.isEmpty()) {
        Logger::log("Triggering kbuildsyscoca6 to update icon cache...");
        QProcess::startDetached(kbuild);
    } else {
        Logger::log("kbuildsyscoca6 not found; skipping cache rebuild.");
        return;
    }
}

void MainWindow::setupMenus() {
    auto *general = menuBar()->addMenu("General");
    auto *viewMenu = menuBar()->addMenu("View");
    auto *window = menuBar()->addMenu("Window");
    auto *system = menuBar()->addMenu("System");
    auto *advanced = menuBar()->addMenu("Advanced");

    // --- View ---
    auto *zoomIn = viewMenu->addAction("Zoom In");
    this->addAction(zoomIn);
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, [this] {
        qreal newZoom = view->zoomFactor() + 0.1;
        newZoom = std::round(newZoom * 10.0) / 10.0;
        view->setZoomFactor(newZoom);
        config.setZoomLevel(newZoom);
    });

    auto *zoomOut = viewMenu->addAction("Zoom Out");
    this->addAction(zoomOut);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, [this] {
        qreal newZoom = view->zoomFactor() - 0.1;
        newZoom = std::round(newZoom * 10.0) / 10.0;
        if (newZoom < 0.25)
            newZoom = 0.25; // Prevent becoming too small
        view->setZoomFactor(newZoom);
        config.setZoomLevel(newZoom);
    });

    auto *zoomReset = viewMenu->addAction("Reset Zoom");
    this->addAction(zoomReset);
    zoomReset->setShortcut(tr("Ctrl+0"));
    connect(zoomReset, &QAction::triggered, [this] {
        view->setZoomFactor(1.0);
        config.setZoomLevel(1.0);
    });

    // --- General ---
    auto *dark = general->addAction("Prefer Dark Mode");
    this->addAction(dark);
    dark->setCheckable(true);
    dark->setChecked(config.preferDarkMode());
    connect(dark, &QAction::toggled, [&](bool v) {
        Logger::log(QString("User toggled Prefer Dark Mode to: %1")
                        .arg(v ? "ENABLED" : "DISABLED"));
        config.setPreferDarkMode(v);
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });

    auto *rememberDl = general->addAction("Remember subsequent Download paths");
    this->addAction(rememberDl);
    rememberDl->setCheckable(true);
    rememberDl->setChecked(config.rememberDownloadPaths());
    connect(rememberDl, &QAction::toggled,
            [&](bool v) { config.setRememberDownloadPaths(v); });

    // --- Window ---
    auto *maxDef = window->addAction("Maximized by Default");
    this->addAction(maxDef);
    maxDef->setCheckable(true);
    maxDef->setChecked(config.maximizedByDefault());
    connect(maxDef, &QAction::toggled,
            [&](bool v) { config.setMaximizedByDefault(v); });

    auto *remember = window->addAction("Remember Window Size");
    this->addAction(remember);
    remember->setCheckable(true);
    remember->setChecked(config.rememberWindowSize());
    connect(remember, &QAction::toggled,
            [&](bool v) { config.setRememberWindowSize(v); });

    auto *trayOpt = window->addAction("Minimize to Tray on Close");
    this->addAction(trayOpt);
    trayOpt->setCheckable(true);
    trayOpt->setChecked(config.minimizeToTray());
    connect(trayOpt, &QAction::toggled,
            [&](bool v) { config.setMinimizeToTray(v); });

    // --- System ---
    auto *autostart = system->addAction("Autostart on Login");
    this->addAction(autostart);
    autostart->setCheckable(true);
    autostart->setChecked(config.autostartOnLogin());
    connect(autostart, &QAction::toggled,
            [&](bool v) { config.setAutostartOnLogin(v); });

    auto *startMin = system->addAction("Start Minimized in Tray");
    this->addAction(startMin);
    startMin->setCheckable(true);
    startMin->setChecked(config.startMinimizedInTray());
    connect(startMin, &QAction::toggled,
            [&](bool v) { config.setStartMinimizedInTray(v); });

    auto *notifications = system->addAction("Enable Notifications");
    this->addAction(notifications);
    notifications->setCheckable(true);
    notifications->setChecked(config.systemNotifications());
    connect(notifications, &QAction::toggled,
            [&](bool v) { config.setSystemNotifications(v); });

    auto *mute = system->addAction("Mute notification sounds");
    this->addAction(mute);
    mute->setCheckable(true);
    mute->setChecked(config.muteAudio());
    connect(mute, &QAction::toggled, [&](bool v) {
        config.setMuteAudio(v);
        web->setAudioMuted(v);
    });

    // --- Advanced ---
    auto *debug = advanced->addAction("Debug: Enable File Logging");
    this->addAction(debug);
    debug->setCheckable(true);
    debug->setChecked(config.debugLoggingEnabled());
    connect(debug, &QAction::toggled, [&](bool v) {
        config.setDebugLoggingEnabled(v);
        Logger::setFileLoggingEnabled(v);
        Logger::log(v ? "File logging ENABLED" : "File logging DISABLED");
    });

    auto *useLessMem = advanced->addAction("Use Less Memory");
    this->addAction(useLessMem);
    useLessMem->setCheckable(true);
    useLessMem->setChecked(config.useLessMemory());
    connect(useLessMem, &QAction::toggled, [this](bool v) {
        config.setUseLessMemory(v);
        updateMemoryState();
    });

    auto *memKill = advanced->addAction("Memory Kill Switch");
    this->addAction(memKill);
    connect(memKill, &QAction::triggered, [this] {
        QDialog dlg(this);
        dlg.setWindowTitle("Memory Kill Switch");
        auto *layout = new QVBoxLayout(&dlg);

        auto *label = new QLabel("Threshold (1GB - 4GB):", &dlg);
        layout->addWidget(label);

        auto *slider = new QSlider(Qt::Horizontal, &dlg);
        slider->setMinimum(0); // 0 means disabled
        slider->setMaximum(4);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(1);
        slider->setValue(config.memoryLimit());

        auto *valueLabel = new QLabel(&dlg);
        auto updateLabel = [valueLabel](int val) {
            if (val == 0)
                valueLabel->setText("Disabled");
            else
                valueLabel->setText(QString("%1 GB").arg(val));
        };
        updateLabel(slider->value());
        connect(slider, &QSlider::valueChanged, updateLabel);

        layout->addWidget(slider);
        layout->addWidget(valueLabel);

        auto *btnBox = new QHBoxLayout;
        auto *saveBtn = new QPushButton("Save", &dlg);
        auto *cancelBtn = new QPushButton("Cancel", &dlg);
        btnBox->addWidget(saveBtn);
        btnBox->addWidget(cancelBtn);
        layout->addLayout(btnBox);

        connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            config.setMemoryLimit(slider->value());
            if (slider->value() > 0) {
                if (!memoryTimer->isActive())
                    memoryTimer->start(30000);
            } else {
                memoryTimer->stop();
            }
        }
    });

    advanced->addSeparator();

    auto *reload = advanced->addAction("Reload Config and Cache", [&] {
        Logger::log("Reloading config and cache...");
        QDir(config.configDir()).removeRecursively();
        QDir(QStandardPaths::writableLocation(
                 QStandardPaths::GenericCacheLocation) +
             "/whatsit")
            .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });
    this->addAction(reload);

    auto *delProfile = advanced->addAction("Delete Profile and Restart", [&] {
        Logger::log("Deleting profile and restarting...");
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });
    this->addAction(delProfile);

    auto *customize = advanced->addAction("Customize App");
    this->addAction(customize);
    connect(customize, &QAction::triggered, [this] {
        QDialog dlg(this);
        dlg.setWindowTitle("Customize App");
        dlg.setMinimumSize(800, 400);

        auto *layout = new QFormLayout(&dlg);

        auto *urlEdit = new QLineEdit(&dlg);
        urlEdit->setText(config.customUrl());
        urlEdit->setPlaceholderText("https://web.whatsapp.com");

        auto *trayIconBtn = new QPushButton("Choose Tray Icon...", &dlg);
        QString currentTrayIcon = config.customTrayIcon();
        QString selectedTrayIcon = currentTrayIcon;

        auto *appIconBtn = new QPushButton("Choose App Icon...", &dlg);
        QString currentAppIcon = config.customAppIcon();
        QString selectedAppIcon = currentAppIcon;

        if (!currentTrayIcon.isEmpty()) {
            trayIconBtn->setText(currentTrayIcon);
            QIcon tempIcon = QIcon::fromTheme(currentTrayIcon);
            if (tempIcon.isNull())
                tempIcon = QIcon(currentTrayIcon);
            trayIconBtn->setIcon(tempIcon);
        }

        if (!currentAppIcon.isEmpty()) {
            appIconBtn->setText(currentAppIcon);
            QIcon tempIcon = QIcon::fromTheme(currentAppIcon);
            if (tempIcon.isNull())
                tempIcon = QIcon(currentAppIcon);
            appIconBtn->setIcon(tempIcon);
        }

        connect(trayIconBtn, &QPushButton::clicked, [&] {
            QString icon = KIconDialog::getIcon(
                KIconLoader::Desktop, KIconLoader::Application, false, 0, false,
                &dlg, "Select Tray Icon");
            if (!icon.isEmpty()) {
                selectedTrayIcon = icon;
                trayIconBtn->setText(icon);
                QIcon tempIcon = QIcon::fromTheme(icon);
                if (tempIcon.isNull())
                    tempIcon = QIcon(icon);
                trayIconBtn->setIcon(tempIcon);
            }
        });
        
        connect(appIconBtn, &QPushButton::clicked, [&] {
            QString icon = KIconDialog::getIcon(
                KIconLoader::Desktop, KIconLoader::Application, false, 0, false,
                &dlg, "Select App Icon");
            if (!icon.isEmpty()) {
                selectedAppIcon = icon;
                appIconBtn->setText(icon);
                QIcon tempIcon = QIcon::fromTheme(icon);
                if (tempIcon.isNull())
                    tempIcon = QIcon(icon);
                appIconBtn->setIcon(tempIcon);
            }
        });

        auto *btns = new QHBoxLayout;
        auto *saveBtn = new QPushButton("Save", &dlg);
        auto *removeBtn = new QPushButton("Remove Customizations", &dlg);
        auto *cancelBtn = new QPushButton("Cancel", &dlg);
        btns->addWidget(saveBtn);
        btns->addWidget(removeBtn);
        btns->addWidget(cancelBtn);

        layout->addRow("App URL:", urlEdit);
        layout->addRow("Tray Icon:", trayIconBtn);
        layout->addRow("App Icon:", appIconBtn);
        layout->addRow(btns);

        connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        connect(removeBtn, &QPushButton::clicked, [&] {
            config.removeCustomConfig();

            // Immediately update the tray icon
            if (tray) {
                tray->setIcon("whatsit");
            }
            QString local_share_apps_dir = QStandardPaths::writableLocation(
                QStandardPaths::ApplicationsLocation);
            QDir appsDir(local_share_apps_dir);
            QString local_desktop_file = appsDir.filePath("whatsit.desktop");
            QFile desktopFile(local_desktop_file);

            Logger::log("Desktop file:");
            Logger::log(local_desktop_file);
            if (!desktopFile.exists()) {
                Logger::log("Desktop file not found; you're on your own.");
            }
            if (!desktopFile.remove()) {
                Logger::log("Failed to remove desktop file.");
            } else {
                Logger::log("Desktop file removed successfully.");
            }
            rebuildKCache();
            QMessageBox::information(
                &dlg, "Customizations Removed",
                "Custom settings have been removed. Restart to see changes.");
            dlg.reject();
        });

        if (dlg.exec() == QDialog::Accepted) {
            config.setCustomUrl(urlEdit->text());
            config.setCustomTrayIcon(selectedTrayIcon);
            config.setCustomAppIcon(selectedAppIcon);

            // Immediately update the tray icon
            if (tray) {
                tray->setIcon(selectedTrayIcon);
            }

            if (!selectedAppIcon.isEmpty()) {
                ensureDesktopFile(selectedAppIcon);
            }

            QMessageBox::information(
                this, "Restart Required",
                "Changes will take effect after restarting the application.");
        }
    });
}
