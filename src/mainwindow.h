// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QUrl>
#include "configmanager.h"

class QWebEngineView;
class WebEngineHelper;
class TrayManager;
class IpcManager;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(ConfigManager& config, QWidget *parent = nullptr); // inherit config from main
    ~MainWindow() override;

  protected:
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

  private slots:
    void checkMemoryUsage();
    void handleIncomingUrl(const QUrl &url);
    void clearSendMessageUrl();
    void handleMessageDetected();
    void handleUnreadChanged(bool hasUnread);
    void startPeriodicCheck();
    void performPeriodicCheck();
    void finishPeriodicCheck();

  private:
    void setupMenus();
    void ensureDesktopFile(const QString &iconPath);
    void rebuildKCache();
    void handleExitRequest();
    void updateMemoryState(bool forceLoad = false);
    QUrl getTargetUrl() const;

    // unified tray/window behavior
    void showAndRaise();

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

  private slots:
    void hideCursor();

  private:

    QWebEngineView *view;
    QUrl sendMessageURL;

    ConfigManager& config;
    WebEngineHelper *web;
    TrayManager *tray;
    IpcManager *ipc;
    QTimer *memoryTimer;
    QTimer *periodicCheckTimer;
    QTimer *activeCheckTimer;
    QTimer *cursorHideTimer;
    bool m_hasUnread = false;
    bool m_isCheckingInMenu = false; // Tracks if we're in the middle of a periodic background check while minimized to tray
};
