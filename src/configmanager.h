// configmanager.h
#pragma once

#include <QMap>
#include <QSize>
#include <QString>

class ConfigManager {
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
    bool muteAudio() const;

    // --- Advanced ---
    bool useLessMemory() const;
    int memoryLimit() const;

    // Debug
    bool debugLoggingEnabled() const;

    // --- Downloads ---
    QString downloadPath() const;

    // --- Custom ---
    QString customUrl() const;
    void setCustomUrl(const QString &url);

    QString customTrayIcon() const;
    void setCustomTrayIcon(const QString &icon);

    QString customAppIcon() const;
    void setCustomAppIcon(const QString &icon);

    void removeCustomConfig();

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
    void setMuteAudio(bool);

    // --- Advanced ---
    void setUseLessMemory(bool);
    void setMemoryLimit(int);

    // Debug
    void setDebugLoggingEnabled(bool);

    void setDownloadPath(const QString &);

    QString configDir() const;

  private:
    QString m_configDir;
    QString m_configPath;

    int m_memoryLimit = 0;

    // Centralized boolean storage
    QMap<QString, bool> m_boolValues;

    void loadBool(const QString &key, bool defaultValue);
    bool boolValue(const QString &key) const;
    void setBoolValue(const QString &key, bool value);

    // Safe autostart handling
    void applyAutostart(bool enabled);
};
