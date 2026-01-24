// webenginehelper.h
#pragma once

#include <QObject>
#include <QWebEngineView>
#include <QWebEngineProfile>

class ConfigManager;
class QWebEngineDownloadRequest;

class WebEngineHelper : public QObject
{
    Q_OBJECT
public:
    explicit WebEngineHelper(QWebEngineView *view,
                             ConfigManager *config,
                             QObject *parent = nullptr);

    void initialize();
    QWebEngineProfile *profile() const;
    void setAudioMuted(bool muted);

private slots:
    void handleDownloadRequested(QWebEngineDownloadRequest *download);

private:
    // void applyTheme();
    QWebEngineView *m_view;
    QWebEngineProfile *m_profile;
    ConfigManager *m_config;
};
