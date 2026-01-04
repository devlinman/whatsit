// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QWebEngineView>

#include "configmanager.h"
#include "webenginehelper.h"
#include "traymanager.h"
#include "ipcmanager.h"

class MainWindow : public QMainWindow
{
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

private:
    void setupMenus();
    void handleExitRequest();
    void updateMemoryState();

    // unified tray/window behavior
    void showAndRaise();

    QWebEngineView *view;

    ConfigManager config;
    WebEngineHelper *web;
    TrayManager *tray;
    IpcManager *ipc;
    QTimer *memoryTimer;
};
