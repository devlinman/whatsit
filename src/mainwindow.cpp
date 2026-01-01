// mainwindow.cpp
#include "mainwindow.h"

#include <QMenuBar>
#include <QCloseEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QShortcut>
#include <cmath>

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
    
    QString iconName = "whatsit";
    
    tray->setIcon(iconName);

    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull()) {
        icon = QIcon(iconName);
    }
    this->setWindowIcon(icon);
    qApp->setWindowIcon(icon);

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

    auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
    quitShortcut->setContext(Qt::ApplicationShortcut);
    connect(quitShortcut, &QShortcut::activated, this, [this] {
        handleExitRequest();
    });

    setupMenus();

    view->load(QUrl("https://web.whatsapp.com"));
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

void MainWindow::setupMenus()
{
    auto *general  = menuBar()->addMenu("General");
    auto *viewMenu = menuBar()->addMenu("View");
    auto *window   = menuBar()->addMenu("Window");
    auto *system   = menuBar()->addMenu("System");
    auto *advanced = menuBar()->addMenu("Advanced");

    // --- View ---
    auto *zoomIn = viewMenu->addAction("Zoom In");
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, [this] {
        qreal newZoom = view->zoomFactor() + 0.1;
        newZoom = std::round(newZoom * 10.0) / 10.0;
        view->setZoomFactor(newZoom);
        config.setZoomLevel(newZoom);
    });

    auto *zoomOut = viewMenu->addAction("Zoom Out");
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, [this] {
        qreal newZoom = view->zoomFactor() - 0.1;
        newZoom = std::round(newZoom * 10.0) / 10.0;
        if (newZoom < 0.25) newZoom = 0.25; // Prevent becoming too small
        view->setZoomFactor(newZoom);
        config.setZoomLevel(newZoom);
    });

    auto *zoomReset = viewMenu->addAction("Reset Zoom");
    zoomReset->setShortcut(tr("Ctrl+0"));
    connect(zoomReset, &QAction::triggered, [this] {
        view->setZoomFactor(1.0);
        config.setZoomLevel(1.0);
    });

    // --- General ---
    auto *dark = general->addAction("Prefer Dark Mode");
    dark->setCheckable(true);
    dark->setChecked(config.preferDarkMode());
    connect(dark, &QAction::toggled, [&](bool v) {
        Logger::log(QString("User toggled Prefer Dark Mode to: %1").arg(v ? "ENABLED" : "DISABLED"));
        config.setPreferDarkMode(v);
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });

    auto *rememberDl = general->addAction("Remember subsequent Download paths");
    rememberDl->setCheckable(true);
    rememberDl->setChecked(config.rememberDownloadPaths());
    connect(rememberDl, &QAction::toggled,
            [&](bool v) { config.setRememberDownloadPaths(v); });

    // --- Window ---
    auto *maxDef = window->addAction("Maximized by Default");
    maxDef->setCheckable(true);
    maxDef->setChecked(config.maximizedByDefault());
    connect(maxDef, &QAction::toggled,
            [&](bool v) { config.setMaximizedByDefault(v); });

    auto *remember = window->addAction("Remember Window Size");
    remember->setCheckable(true);
    remember->setChecked(config.rememberWindowSize());
    connect(remember, &QAction::toggled,
            [&](bool v) { config.setRememberWindowSize(v); });

    auto *trayOpt = window->addAction("Minimize to Tray on Close");
    trayOpt->setCheckable(true);
    trayOpt->setChecked(config.minimizeToTray());
    connect(trayOpt, &QAction::toggled,
            [&](bool v) { config.setMinimizeToTray(v); });

    // --- System ---
    auto *autostart = system->addAction("Autostart on Login");
    autostart->setCheckable(true);
    autostart->setChecked(config.autostartOnLogin());
    connect(autostart, &QAction::toggled,
            [&](bool v) { config.setAutostartOnLogin(v); });

    auto *startMin = system->addAction("Start Minimized in Tray");
    startMin->setCheckable(true);
    startMin->setChecked(config.startMinimizedInTray());
    connect(startMin, &QAction::toggled,
            [&](bool v) { config.setStartMinimizedInTray(v); });

    auto *notifications = system->addAction("Enable Notifications");
    notifications->setCheckable(true);
    notifications->setChecked(config.systemNotifications());
    connect(notifications, &QAction::toggled,
            [&](bool v) { config.setSystemNotifications(v); });

    // --- Advanced ---
    auto *debug = advanced->addAction("Debug: Enable File Logging");
    debug->setCheckable(true);
    debug->setChecked(config.debugLoggingEnabled());
    connect(debug, &QAction::toggled, [&](bool v) {
        config.setDebugLoggingEnabled(v);
        Logger::setFileLoggingEnabled(v);
        Logger::log(v ? "File logging ENABLED" : "File logging DISABLED");
    });

    advanced->addSeparator();

    advanced->addAction("Reload Config and Cache", [&] {
        Logger::log("Reloading config and cache...");
        QDir(config.configDir()).removeRecursively();
        QDir(QStandardPaths::writableLocation(
            QStandardPaths::GenericCacheLocation) + "/whatsit")
        .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });

    advanced->addAction("Delete Profile and Restart", [&] {
        Logger::log("Deleting profile and restarting...");
        QDir(QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation))
        .removeRecursively();
        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });
}
