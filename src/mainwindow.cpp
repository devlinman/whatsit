// mainwindow.cpp
#include "mainwindow.h"

#include <QMenuBar>
#include <QCloseEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QShortcut>
#include <cmath>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QUrl>
#include <KIconDialog>
#include <KIconLoader>
#include <QSlider>
#include <QTimer>
#include <unistd.h>

#include "logger.h"

static constexpr int DEFAULT_W = 1200;
static constexpr int DEFAULT_H = 800;

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
view(new QWebEngineView(this)),
web(nullptr),
tray(nullptr),
ipc(nullptr)
{
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
    
    QString iconToUse = "whatsit";
    QString customIconStr = config.customIcon();
    
    if (!customIconStr.isEmpty()) {
        iconToUse = customIconStr;
    } else {
        // Legacy: Check for custom icon file
        QString customIconPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/hicolor/256x256/apps/whatsit_custom.png";
        if (QFile::exists(customIconPath)) {
            iconToUse = customIconPath;
        }
    }
    
    QIcon icon = QIcon::fromTheme(iconToUse);
    if (icon.isNull()) icon = QIcon(iconToUse);
    
    if (icon.isNull() && iconToUse != "whatsit") {
        iconToUse = "whatsit";
        icon = QIcon::fromTheme(iconToUse);
        if (icon.isNull()) icon = QIcon(iconToUse);
    }
    
    this->setWindowIcon(icon);
    qApp->setWindowIcon(icon);
    tray->setIcon(iconToUse);

    connect(tray, &TrayManager::showRequested,
            this, &MainWindow::showAndRaise);
    connect(tray, &TrayManager::hideRequested,
            this, &QWidget::hide);
    connect(tray, &TrayManager::activated, this, [this] {
        if (isVisible() && isActiveWindow() && !isMinimized()) {
            hide();
        } else {
            showAndRaise();
        }
    });

    ipc = new IpcManager(this);
    connect(ipc, &IpcManager::raiseRequested,
            this, &MainWindow::showAndRaise);
    ipc->start();

    memoryTimer = new QTimer(this);
    connect(memoryTimer, &QTimer::timeout, this, &MainWindow::checkMemoryUsage);
    if (config.memoryLimit() > 0) {
        memoryTimer->start(30000); // Check every 30 seconds
    }

    auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
    quitShortcut->setContext(Qt::ApplicationShortcut);
    connect(quitShortcut, &QShortcut::activated, this, [this] {
        handleExitRequest();
    });

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
         Logger::log("Invalid Custom URL: " + targetUrlStr + " -> Fallback to Google.");
         targetUrl = QUrl("https://google.com");
    }

    Logger::log("Loading URL: " + targetUrl.toString());
    view->load(targetUrl);
}

MainWindow::~MainWindow()
{
    if (config.rememberWindowSize())
        config.setWindowSize(size());

    config.sync();
}

// unified show / raise behavior
void MainWindow::showAndRaise()
{
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    
    show();
    raise();
    activateWindow();
}

// SINGLE exit decision point
void MainWindow::handleExitRequest()
{
    if (config.minimizeToTray()) {
        Logger::log("Minimizing to tray instead of quitting.");
        hide();
    } else {
        Logger::log("Quitting application.");
        qApp->quit();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    updateMemoryState();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    updateMemoryState();
}

void MainWindow::updateMemoryState()
{
    if (!view) return;

    if (config.useLessMemory() && !isVisible()) {
        // If hidden and memory optimization is ON, unload the page
        if (view->url().toString() != "about:blank") {
            Logger::log("Use Less Memory: Unloading content to about:blank");
            view->stop(); // Stop any pending loads
            view->setUrl(QUrl("about:blank"));
        }
    } else {
        // If visible OR memory optimization is OFF, ensure we have the correct page
        if (view->url().toString() == "about:blank") {
             Logger::log("Use Less Memory: Restoring content");
             QString target = config.customUrl();
             if (target.isEmpty()) target = "https://web.whatsapp.com";
             view->setUrl(QUrl(target));
        }
    }
}

void MainWindow::checkMemoryUsage()
{
    int limitGb = config.memoryLimit();
    if (limitGb <= 0) return;

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
        Logger::log(QString("MEMORY KILL SWITCH TRIGGERED: %1 GB used, limit is %2 GB. Quitting.").arg(totalGb, 0, 'f', 2).arg(limitGb));
        qApp->quit();
    }
}

void MainWindow::setupMenus()
{
    auto *general  = menuBar()->addMenu("General");
    auto *viewMenu = menuBar()->addMenu("View");
    auto *window   = menuBar()->addMenu("Window");
    auto *system   = menuBar()->addMenu("System");
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
        if (newZoom < 0.25) newZoom = 0.25; // Prevent becoming too small
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
        Logger::log(QString("User toggled Prefer Dark Mode to: %1").arg(v ? "ENABLED" : "DISABLED"));
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
            if (val == 0) valueLabel->setText("Disabled");
            else valueLabel->setText(QString("%1 GB").arg(val));
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
                if (!memoryTimer->isActive()) memoryTimer->start(30000);
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
            QStandardPaths::GenericCacheLocation) + "/whatsit")
        .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });
    this->addAction(reload);

    auto *delProfile = advanced->addAction("Delete Profile and Restart", [&] {
        Logger::log("Deleting profile and restarting...");
        QDir(QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation))
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
        dlg.setMinimumSize(400, 200);
        
        auto *layout = new QFormLayout(&dlg);
        
        auto *urlEdit = new QLineEdit(&dlg);
        urlEdit->setText(config.customUrl());
        urlEdit->setPlaceholderText("https://web.whatsapp.com");
        
        auto *iconBtn = new QPushButton("Choose Icon...", &dlg);
        QString currentIcon = config.customIcon();
        QString selectedIcon = currentIcon;

        if (!currentIcon.isEmpty()) {
             iconBtn->setText(currentIcon);
             QIcon tempIcon = QIcon::fromTheme(currentIcon);
             if (tempIcon.isNull()) tempIcon = QIcon(currentIcon);
             iconBtn->setIcon(tempIcon);
        }

        connect(iconBtn, &QPushButton::clicked, [&] {
            QString icon = KIconDialog::getIcon(KIconLoader::Desktop, KIconLoader::Application, false, 0, false, &dlg, "Select Icon");
            if (!icon.isEmpty()) {
                selectedIcon = icon;
                iconBtn->setText(icon);
                QIcon tempIcon = QIcon::fromTheme(icon);
                if (tempIcon.isNull()) tempIcon = QIcon(icon);
                iconBtn->setIcon(tempIcon);
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
        layout->addRow("App Icon:", iconBtn);
        layout->addRow(btns);
        
        connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        
        connect(removeBtn, &QPushButton::clicked, [&] {
             config.removeCustomConfig();
             QMessageBox::information(&dlg, "Customizations Removed", "Custom settings have been removed. Restart to see changes.");
             dlg.reject();
        });
        
        if (dlg.exec() == QDialog::Accepted) {
            config.setCustomUrl(urlEdit->text());
            config.setCustomIcon(selectedIcon);
            
            QMessageBox::information(this, "Restart Required", "Changes will take effect after restarting the application.");
        }
    });
}
