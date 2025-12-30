#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QSettings>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupWebEngine();
    void applyDarkModeIfNeeded();

    QWebEngineView *view;
    QWebEngineProfile *profile;
    QSettings settings;
};
