#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QSettings>
#include <QCloseEvent>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupWebEngine();
    void applyDarkModeIfNeeded();

    void initializeConfigDefaults();
    void restoreWindowFromConfig();
    void saveWindowToConfig();

    QWebEngineView *view;
    QWebEngineProfile *profile;
    QSettings settings;
};
