// configmanager.cpp
#include "configmanager.h"
#include "logger.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <cmath>

// ---------------- Constructor ----------------

ConfigManager::ConfigManager()
{
    m_configDir =
    QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
    + "/whatsit";

    QDir().mkpath(m_configDir);

    m_configPath = m_configDir + "/whatsit.ini";
}

// ---------------- Lifecycle ----------------

void ConfigManager::load()
{
    Logger::log("Loading configuration...");
    // --- Boolean schema ---
    loadBool("General/PreferDarkMode", false);
    loadBool("General/RememberDownloadPaths", true);

    loadBool("Window/MaximizedByDefault", false);
    loadBool("Window/RememberWindowSize", true);

    loadBool("System/AutostartOnLogin", false);
    loadBool("System/MinimizeToTray", true);
    loadBool("System/StartMinimizedInTray", false);
    loadBool("System/SystemNotifications", true);

    loadBool("Debug/EnableFileLogging", false);

    // Ensure autostart state is reflected on disk
    applyAutostart(boolValue("System/AutostartOnLogin"));

    sync();
}

void ConfigManager::sync()
{
    Logger::log("Syncing configuration to disk: " + m_configPath);
    QSettings settings(m_configPath, QSettings::IniFormat);

    for (auto it = m_boolValues.constBegin(); it != m_boolValues.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }

    settings.sync();
}

// ---------------- Getters ----------------

bool ConfigManager::preferDarkMode() const
{
    return boolValue("General/PreferDarkMode");
}

bool ConfigManager::rememberDownloadPaths() const
{
    return boolValue("General/RememberDownloadPaths");
}

bool ConfigManager::maximizedByDefault() const
{
    return boolValue("Window/MaximizedByDefault");
}

bool ConfigManager::rememberWindowSize() const
{
    return boolValue("Window/RememberWindowSize");
}

QSize ConfigManager::windowSize() const
{
    return QSettings(m_configPath, QSettings::IniFormat)
    .value("Window/Size", QSize(900, 600))
    .toSize();
}

double ConfigManager::zoomLevel() const
{
    return QSettings(m_configPath, QSettings::IniFormat)
    .value("Window/ZoomLevel", 1.0)
    .toDouble();
}

bool ConfigManager::autostartOnLogin() const
{
    return boolValue("System/AutostartOnLogin");
}

bool ConfigManager::minimizeToTray() const
{
    return boolValue("System/MinimizeToTray");
}

bool ConfigManager::startMinimizedInTray() const
{
    return boolValue("System/StartMinimizedInTray");
}

bool ConfigManager::systemNotifications() const
{
    return boolValue("System/SystemNotifications");
}

bool ConfigManager::debugLoggingEnabled() const
{
    return boolValue("Debug/EnableFileLogging");
}

QString ConfigManager::downloadPath() const
{
    return QSettings(m_configPath, QSettings::IniFormat)
    .value("Downloads/DownloadPath",
           QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
    .toString();
}

// ---------------- Setters ----------------

void ConfigManager::setPreferDarkMode(bool v)
{
    setBoolValue("General/PreferDarkMode", v);
}

void ConfigManager::setRememberDownloadPaths(bool v)
{
    setBoolValue("General/RememberDownloadPaths", v);
}

void ConfigManager::setMaximizedByDefault(bool v)
{
    setBoolValue("Window/MaximizedByDefault", v);
}

void ConfigManager::setRememberWindowSize(bool v)
{
    setBoolValue("Window/RememberWindowSize", v);
}

void ConfigManager::setWindowSize(const QSize &size)
{
    QSettings(m_configPath, QSettings::IniFormat)
    .setValue("Window/Size", size);
}

void ConfigManager::setZoomLevel(double level)
{
    // Ensure we store a clean 1-decimal value
    double rounded = std::round(level * 10.0) / 10.0;
    QSettings(m_configPath, QSettings::IniFormat)
    .setValue("Window/ZoomLevel", rounded);
}

void ConfigManager::setAutostartOnLogin(bool v)
{
    setBoolValue("System/AutostartOnLogin", v);
    applyAutostart(v);
}

void ConfigManager::setMinimizeToTray(bool v)
{
    setBoolValue("System/MinimizeToTray", v);
}

void ConfigManager::setStartMinimizedInTray(bool v)
{
    setBoolValue("System/StartMinimizedInTray", v);
}

void ConfigManager::setSystemNotifications(bool v)
{
    setBoolValue("System/SystemNotifications", v);
}

void ConfigManager::setDebugLoggingEnabled(bool v)
{
    setBoolValue("Debug/EnableFileLogging", v);
}

void ConfigManager::setDownloadPath(const QString &path)
{
    QSettings(m_configPath, QSettings::IniFormat)
    .setValue("Downloads/DownloadPath", path);
}

// ---------------- Paths ----------------

QString ConfigManager::configDir() const
{
    return m_configDir;
}

// ---------------- Internal helpers ----------------

void ConfigManager::loadBool(const QString &key, bool defaultValue)
{
    QSettings settings(m_configPath, QSettings::IniFormat);

    bool value = settings.value(key, defaultValue).toBool();
    m_boolValues.insert(key, value);
    settings.setValue(key, value);
}

bool ConfigManager::boolValue(const QString &key) const
{
    return m_boolValues.value(key, false);
}

void ConfigManager::setBoolValue(const QString &key, bool value)
{
    m_boolValues[key] = value;

    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.setValue(key, value);
}

// ---------------- Autostart (SAFE) ----------------

void ConfigManager::applyAutostart(bool enabled)
{
    Logger::log(QString("Applying autostart: %1").arg(enabled ? "ENABLED" : "DISABLED"));
    const QString autostartDir =
    QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
    + "/autostart";

    const QString desktopFile = autostartDir + "/whatsit.desktop";

    if (enabled) {
        QDir().mkpath(autostartDir);

        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Version=1.0\n";
            out << "Name=whatsit\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=whatsit\n";
            out << "Terminal=false\n";
            out << "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        QFile::remove(desktopFile); // removes only this file
    }
}
