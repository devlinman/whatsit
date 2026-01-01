// configmanager.h
#pragma once

#include <QString>
#include <QSize>
#include <QMap>

class ConfigManager
{
public:
    ConfigManager();

    void load();
    void sync();

    // --- General ---
    bool preferDarkMode() const;
    bool rememberDownloadPaths() const;

    // --- Window ---
    bool maximizedByDefault() const;
    bool rememberWindowSize() const;
    QSize windowSize() const;
    double zoomLevel() const;

    // --- System ---
    bool autostartOnLogin() const;
    bool minimizeToTray() const;
    bool startMinimizedInTray() const;
    bool systemNotifications() const;

    // Debug
    bool debugLoggingEnabled() const;

    // --- Downloads ---
    QString downloadPath() const;

    // --- Setters ---
    void setPreferDarkMode(bool);
    void setRememberDownloadPaths(bool);

    void setMaximizedByDefault(bool);
    void setRememberWindowSize(bool);
    void setWindowSize(const QSize &);
    void setZoomLevel(double);

    void setAutostartOnLogin(bool);
    void setMinimizeToTray(bool);
    void setStartMinimizedInTray(bool);
    void setSystemNotifications(bool);

    // Debug
    void setDebugLoggingEnabled(bool);

    void setDownloadPath(const QString &);

    QString configDir() const;

private:
    QString m_configDir;
    QString m_configPath;

    // Centralized boolean storage
    QMap<QString, bool> m_boolValues;

    void loadBool(const QString &key, bool defaultValue);
    bool boolValue(const QString &key) const;
    void setBoolValue(const QString &key, bool value);

    // Safe autostart handling
    void applyAutostart(bool enabled);
};
