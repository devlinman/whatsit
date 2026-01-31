// mainwindow.cpp
#include "mainwindow.h"

#include <KIconDialog>
#include <KIconLoader>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
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
#include <QUrlQuery>
#include <QVBoxLayout>
#include <cmath>
#include <unistd.h>
#include <QColor>
#include <QWebEngineView>
#include "logger.h"
#include "ipcmanager.h"
#include "traymanager.h"
#include "webenginehelper.h"

static constexpr int DEFAULT_W = 1200;
static constexpr int DEFAULT_H = 800;
// <html><body style="background-color: #1e1e1e;"></body></html>
static const QUrl DARK_BLANK_URL("data:text/html;base64,PGh0bWw+PGJvZHkgc3R5bGU9ImJhY2tncm91bmQtY29sb3I6ICMxZTFlMWU7Ij48L2JvZHk+PC9odG1sPg==");

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
    
    // Set background color to dark to prevent flashbangs
    if (view->page()) {
        view->page()->setBackgroundColor(QColor("#1e1e1e"));
    }

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
    connect(ipc, &IpcManager::hideRequested, this, &MainWindow::hide);
    connect(ipc, &IpcManager::openUrlRequested, this,
            &MainWindow::handleIncomingUrl);
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

    QUrl targetUrl = getTargetUrl();

    if (config.useLessMemory() && config.startMinimizedInTray()) {
        Logger::log("Low-memory startup: Delaying load of " +
                    targetUrl.toString());
        view->setUrl(DARK_BLANK_URL);
    } else {
        Logger::log("Loading URL: " + targetUrl.toString());
        view->load(targetUrl);
    }

    // Check for command line URL override
    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].startsWith("http") || args[i].startsWith("whatsapp")) {
            // Debug:
            // Logger::log("Processing URL override from command line: " +
            // args[i]);
            Logger::log("Processing URL override from command line...");
            handleIncomingUrl(QUrl(args[i]));
            break;
        }
    }
}

MainWindow::~MainWindow() {
    clearSendMessageUrl();
    if (config.rememberWindowSize())
        config.setWindowSize(size());

    config.sync();
}

void MainWindow::clearSendMessageUrl() {
    if (!sendMessageURL.isEmpty()) {
        sendMessageURL.clear();
        Logger::log("Lifecycle Event: sendMessageURL cleared from memory.");
    }
}

void MainWindow::handleIncomingUrl(const QUrl &url) {
    // Logger::log("Handling incoming URL: " + url.toString());
    showAndRaise();

    if (!url.isValid()) {
        Logger::log("Malformed URL: Invalid.");
        return;
    }

    QUrl finalUrl = url;

    if (url.scheme() == "whatsapp") {
        if (url.host() == "send") {
            finalUrl = QUrl("https://web.whatsapp.com/send/");
            finalUrl.setQuery(url.query());
        }
    }

    if (finalUrl.host() == "web.whatsapp.com") {
        QString path = finalUrl.path();
        if (path == "/" || path.isEmpty()) {
            Logger::log("Base URL requested. Only focus window.");
            return;
        }

        // Handle /send or /send/
        if (path.startsWith("/send")) {
            QUrlQuery query(finalUrl);
            if (query.hasQueryItem("text")) {
                QString text = query.queryItemValue("text");
                // Debug: Log text parameter
                // Logger::log("Send intent detected. Text: " + text);
            } else {
                // Debug:
                // Logger::log("Send intent missing 'text' parameter.");
            }

            sendMessageURL = finalUrl;
            // Debug:
            // Logger::log("Stored sendMessageURL: " +
            // sendMessageURL.toString());
            Logger::log("Recieved valid sendMessageURL");

            view->load(sendMessageURL);

            return;
        }
    }

    Logger::log("Handle URl::Malformed or unrecognised URL format. Ignoring.");
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
        clearSendMessageUrl();
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
        clearSendMessageUrl();
        Logger::log("Close event accepted -> Quitting.");
        event->accept();
        qApp->quit();
    }
}

void MainWindow::hideEvent(QHideEvent *event) {
    QMainWindow::hideEvent(event);
    clearSendMessageUrl();
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
        if (view->url() != DARK_BLANK_URL && view->url().toString() != "about:blank") {
            Logger::log("Use Less Memory: Unloading content to dark blank page");
            view->stop(); // Stop any pending loads
            view->setUrl(DARK_BLANK_URL);
        }
    } else {
        // If visible OR memory optimization is OFF, ensure we have the correct page
        if (view->url().toString() == "about:blank" || view->url() == DARK_BLANK_URL) {
            Logger::log("Use Less Memory: Restoring content");
            if (sendMessageURL.isValid()) {
                view->setUrl(sendMessageURL);
            } else {
                view->setUrl(getTargetUrl());
            }
        }
    }
}

QUrl MainWindow::getTargetUrl() const {
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
    return targetUrl;
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
            out << "Name=Whatsit\n";
            out << "Comment=WhatsApp Web Client\n";
            out << "Exec=" << QCoreApplication::applicationFilePath()
                << " %u\n";
            out << "Icon=" << iconPath << "\n";
            out << "Terminal=false\n";
            out << "Categories=Network;Chat;\n";
            out << "StartupNotify=true\n";
            out << "StartupWMClass=whatsit\n";
            out << "X-KDE-Notifications=whatsit\n";
            out << "MimeType=x-scheme-handler/whatsapp;x-scheme-handler/"
                   "whatsit;\n";
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
    QString kbuild = QStandardPaths::findExecutable("kbuildsycoca6");
    if (kbuild.isEmpty()) {
        kbuild = QStandardPaths::findExecutable("kbuildsycoca5");
    }

    if (!kbuild.isEmpty()) {
        Logger::log("Triggering " + kbuild + " to update icon cache...");
        QProcess::startDetached(kbuild);
    } else {
        Logger::log("kbuildsycoca6/5 not found; skipping cache rebuild.");
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
    auto *zoomIn = viewMenu->addAction(
        QIcon::fromTheme("zoom-in"),
        "Zoom In");
    this->addAction(zoomIn);
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, [this] {
        qreal newZoom = view->zoomFactor() + 0.1;
        newZoom = std::round(newZoom * 10.0) / 10.0;
        view->setZoomFactor(newZoom);
        config.setZoomLevel(newZoom);
    });

    auto *zoomOut = viewMenu->addAction(
        QIcon::fromTheme("zoom-out"),
        "Zoom Out");
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

    auto *zoomReset = viewMenu->addAction(
        QIcon::fromTheme("zoom-original"),
        "Reset Zoom");
    this->addAction(zoomReset);
    zoomReset->setShortcut(tr("Ctrl+0"));
    connect(zoomReset, &QAction::triggered, [this] {
        view->setZoomFactor(1.0);
        config.setZoomLevel(1.0);
    });

    // --- General ---
    auto *rememberDl = general->addAction(
        QIcon::fromTheme("download"),
        "Remember subsequent Download paths");
    this->addAction(rememberDl);
    rememberDl->setCheckable(true);
    rememberDl->setChecked(config.rememberDownloadPaths());
    connect(rememberDl, &QAction::toggled,
            [&](bool v) { config.setRememberDownloadPaths(v); });

    general->addSeparator();

    auto *aboutAction = general->addAction(
        QIcon::fromTheme("help-about"),
        "About Whatsit");
    this->addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this, "About Whatsit",
            "<h2>Whatsit</h2>"
            "<p>A lightweight, native desktop client for WhatsApp Web.</p>"
            "<p><b>Developer:</b> devlinman</p>"
            "<p><b>GitHub:</b> "
            "<a href=\"https://github.com/devlinman/whatsit\">"
            "https://github.com/devlinman/whatsit"
            "</a></p>"
            "<p><b>License:</b> MIT License</p>"
            "<p>Check the LICENSE file for more details.</p>"
            "<br>"
            "<h3>Keybindings:</h3>"
            "<ul>"
            "<li><b>Ctrl+Q</b> — Quit or hide the app (depending on "
            "configuration)</li>"
            "<li><b>Ctrl+Shift+Q</b> — Quit the app completely</li>"
            "</ul>"
        );
    });
    auto *quitAction = general->addAction(
        QIcon::fromTheme("application-exit"),
        "Quit App");
    this->addAction(quitAction);
    connect(quitAction, &QAction::triggered, [this] { qApp->quit(); });

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

    auto *mute = system->addAction("Mute Sounds");
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

    auto *memKill = advanced->addAction(
        QIcon::fromTheme("computer"),
        "Memory Kill Switch");
    this->addAction(memKill);
    connect(memKill, &QAction::triggered, [this] {
        QDialog dlg(this);
        dlg.setWindowTitle("Memory Kill Switch");
        dlg.setMinimumSize(600, 300);
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

    auto *reload = advanced->addAction(
        QIcon::fromTheme("view-refresh"),
        "Reload Config and Cache", [&] {
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

    auto *delProfile = advanced->addAction(
        QIcon::fromTheme("edit-delete"),
        "Delete Profile and Restart", [&] {
        Logger::log("Deleting profile and restarting...");
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });
    this->addAction(delProfile);

    auto *customize = advanced->addAction(
        QIcon::fromTheme("configure"),
        "Customize App");
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
        // Push buttons to bottom
        layout->addItem(new QSpacerItem(
            0, 0,
            QSizePolicy::Minimum,
            QSizePolicy::Expanding
        ));

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
