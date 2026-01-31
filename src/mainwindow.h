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
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

  protected:
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private slots:
    void checkMemoryUsage();
    void handleIncomingUrl(const QUrl &url);
    void clearSendMessageUrl();

  private:
    void setupMenus();
    void ensureDesktopFile(const QString &iconPath);
    void rebuildKCache();
    void handleExitRequest();
    void updateMemoryState();
    QUrl getTargetUrl() const;

    // unified tray/window behavior
    void showAndRaise();

    QWebEngineView *view;
    QUrl sendMessageURL;

    ConfigManager config;
    WebEngineHelper *web;
    TrayManager *tray;
    IpcManager *ipc;
    QTimer *memoryTimer;
};
