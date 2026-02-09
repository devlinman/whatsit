// configmanager.cpp
#include "configmanager.h"
#include "logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <cmath>

// ---------------- Constructor ----------------

ConfigManager::ConfigManager() {
    m_configDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
        "/whatsit";

    QDir().mkpath(m_configDir);

    m_configPath = m_configDir + "/whatsit.ini";
}

// ---------------- Lifecycle ----------------

void ConfigManager::load() {
    Logger::log("Loading configuration...");
    // --- Boolean schema ---
    loadBool("General/RememberDownloadPaths", true);
    loadBool("General/ShowTrayTooltip", true);

    loadBool("Window/MaximizedByDefault", false);
    loadBool("Window/RememberWindowSize", true);

    // Migration: MinimizeToTray moved from System to Window
    QSettings settings(m_configPath, QSettings::IniFormat);
    if (settings.contains("System/MinimizeToTray") &&
        !settings.contains("Window/MinimizeToTray")) {
        bool val = settings.value("System/MinimizeToTray", true).toBool();
        settings.remove("System/MinimizeToTray");
        settings.setValue("Window/MinimizeToTray", val);
    }
    loadBool("Window/MinimizeToTray", true);

    loadBool("System/AutostartOnLogin", false);
    loadBool("System/StartMinimizedInTray", false);
    loadBool("System/ShowTrayIndicator", true);
    loadBool("System/SystemNotifications", true);
    loadBool("System/MuteAudio", false);

    loadBool("Advanced/UseLessMemory", false);

    QSettings settings_adv(m_configPath, QSettings::IniFormat);
    int memLimit = settings_adv.value("Advanced/MemoryLimit", 0).toInt();
    m_memoryLimit = memLimit;

    m_backgroundCheckInterval = settings_adv.value("Advanced/BackgroundCheckInterval", 0).toInt();

    loadBool("Debug/EnableFileLogging", false);

    // Ensure autostart state is reflected on disk
    applyAutostart(boolValue("System/AutostartOnLogin"));

    sync();
}

void ConfigManager::sync() {
    Logger::log("Syncing configuration to disk: " + m_configPath);
    QSettings settings(m_configPath, QSettings::IniFormat);

    for (auto it = m_boolValues.constBegin(); it != m_boolValues.constEnd();
         ++it) {
        settings.setValue(it.key(), it.value());
    }

    settings.sync();
}

// ---------------- Getters ----------------

bool ConfigManager::rememberDownloadPaths() const {
    return boolValue("General/RememberDownloadPaths");
}

bool ConfigManager::showTrayTooltip() const {
    return boolValue("General/ShowTrayTooltip");
}

bool ConfigManager::maximizedByDefault() const {
    return boolValue("Window/MaximizedByDefault");
}

bool ConfigManager::rememberWindowSize() const {
    return boolValue("Window/RememberWindowSize");
}

QSize ConfigManager::windowSize() const {
    return QSettings(m_configPath, QSettings::IniFormat)
        .value("Window/Size", QSize(900, 600))
        .toSize();
}

double ConfigManager::zoomLevel() const {
    return QSettings(m_configPath, QSettings::IniFormat)
        .value("Window/ZoomLevel", 1.0)
        .toDouble();
}

bool ConfigManager::autostartOnLogin() const {
    return boolValue("System/AutostartOnLogin");
}

bool ConfigManager::minimizeToTray() const {
    return boolValue("Window/MinimizeToTray");
}

bool ConfigManager::startMinimizedInTray() const {
    return boolValue("System/StartMinimizedInTray");
}

bool ConfigManager::showTrayIndicator() const {
    return boolValue("System/ShowTrayIndicator");
}

bool ConfigManager::systemNotifications() const {
    return boolValue("System/SystemNotifications");
}

bool ConfigManager::muteAudio() const { return boolValue("System/MuteAudio"); }

bool ConfigManager::useLessMemory() const {
    return boolValue("Advanced/UseLessMemory");
}

int ConfigManager::memoryLimit() const { return m_memoryLimit; }

int ConfigManager::backgroundCheckInterval() const {
    return m_backgroundCheckInterval;
}

bool ConfigManager::debugLoggingEnabled() const {
    return boolValue("Debug/EnableFileLogging");
}

QString ConfigManager::downloadPath() const {
    return QSettings(m_configPath, QSettings::IniFormat)
        .value("Downloads/DownloadPath", QStandardPaths::writableLocation(
                                             QStandardPaths::DownloadLocation))
        .toString();
}

QString ConfigManager::customUrl() const {
    QString customPath = m_configDir + "/custom.ini";
    return QSettings(customPath, QSettings::IniFormat)
        .value("Custom/Url", "")
        .toString();
}

void ConfigManager::setCustomUrl(const QString &url) {
    QString customPath = m_configDir + "/custom.ini";
    QSettings settings(customPath, QSettings::IniFormat);
    settings.setValue("Custom/Url", url);
    settings.sync();
}

QString ConfigManager::customTrayIcon() const {
    QString customPath = m_configDir + "/custom.ini";
    return QSettings(customPath, QSettings::IniFormat)
        .value("Custom/TrayIcon", "")
        .toString();
}

QString ConfigManager::customAppIcon() const {
    QString customPath = m_configDir + "/custom.ini";
    return QSettings(customPath, QSettings::IniFormat)
        .value("Custom/AppIcon", "")
        .toString();
}

void ConfigManager::setCustomTrayIcon(const QString &icon) {
    QString customPath = m_configDir + "/custom.ini";
    QSettings settings(customPath, QSettings::IniFormat);
    settings.setValue("Custom/TrayIcon", icon);
    settings.sync();
}
void ConfigManager::setCustomAppIcon(const QString &icon) {
    QString customPath = m_configDir + "/custom.ini";
    QSettings settings(customPath, QSettings::IniFormat);
    settings.setValue("Custom/AppIcon", icon);
    settings.sync();
}

void ConfigManager::removeCustomConfig() {
    QString customPath = m_configDir + "/custom.ini";
    if (QFile::exists(customPath)) {
        QFile::remove(customPath);
    }
}

// ---------------- Setters ----------------

void ConfigManager::setRememberDownloadPaths(bool v) {
    setBoolValue("General/RememberDownloadPaths", v);
}

void ConfigManager::setShowTrayTooltip(bool v) {
    setBoolValue("General/ShowTrayTooltip", v);
}

void ConfigManager::setMaximizedByDefault(bool v) {
    setBoolValue("Window/MaximizedByDefault", v);
}

void ConfigManager::setRememberWindowSize(bool v) {
    setBoolValue("Window/RememberWindowSize", v);
}

void ConfigManager::setWindowSize(const QSize &size) {
    QSettings(m_configPath, QSettings::IniFormat).setValue("Window/Size", size);
}

void ConfigManager::setZoomLevel(double level) {
    // Ensure we store a clean 1-decimal value
    double rounded = std::round(level * 10.0) / 10.0;
    QSettings(m_configPath, QSettings::IniFormat)
        .setValue("Window/ZoomLevel", rounded);
}

void ConfigManager::setAutostartOnLogin(bool v) {
    setBoolValue("System/AutostartOnLogin", v);
    applyAutostart(v);
}

void ConfigManager::setMinimizeToTray(bool v) {
    setBoolValue("Window/MinimizeToTray", v);
}

void ConfigManager::setStartMinimizedInTray(bool v) {
    setBoolValue("System/StartMinimizedInTray", v);
}

void ConfigManager::setShowTrayIndicator(bool v) {
    setBoolValue("System/ShowTrayIndicator", v);
}

void ConfigManager::setSystemNotifications(bool v) {
    setBoolValue("System/SystemNotifications", v);
}

void ConfigManager::setMuteAudio(bool v) {
    setBoolValue("System/MuteAudio", v);
}

void ConfigManager::setUseLessMemory(bool v) {
    setBoolValue("Advanced/UseLessMemory", v);
}

void ConfigManager::setMemoryLimit(int limit) {
    m_memoryLimit = limit;
    QSettings(m_configPath, QSettings::IniFormat)
        .setValue("Advanced/MemoryLimit", limit);
}

void ConfigManager::setBackgroundCheckInterval(int interval) {
    m_backgroundCheckInterval = interval;
    QSettings(m_configPath, QSettings::IniFormat)
        .setValue("Advanced/BackgroundCheckInterval", interval);
}

void ConfigManager::setDebugLoggingEnabled(bool v) {
    setBoolValue("Debug/EnableFileLogging", v);
}

void ConfigManager::setDownloadPath(const QString &path) {
    QSettings(m_configPath, QSettings::IniFormat)
        .setValue("Downloads/DownloadPath", path);
}

// ---------------- Paths ----------------

QString ConfigManager::configDir() const { return m_configDir; }

// ---------------- Internal helpers ----------------

void ConfigManager::loadBool(const QString &key, bool defaultValue) {
    QSettings settings(m_configPath, QSettings::IniFormat);

    bool value = settings.value(key, defaultValue).toBool();
    m_boolValues.insert(key, value);
    settings.setValue(key, value);
}

bool ConfigManager::boolValue(const QString &key) const {
    return m_boolValues.value(key, false);
}

void ConfigManager::setBoolValue(const QString &key, bool value) {
    m_boolValues[key] = value;

    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.setValue(key, value);
}

// ---------------- Autostart ----------------

void ConfigManager::applyAutostart(bool enabled) {
    Logger::log(QString("Applying autostart: %1")
                    .arg(enabled ? "ENABLED" : "DISABLED"));
    const QString autostartDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
        "/autostart";

    const QString desktopFile = autostartDir + "/whatsit.desktop";

    if (enabled) {
        QDir().mkpath(autostartDir);

        QFile file(desktopFile);
        // No error handling here. what if file.open faisl? #TODO
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Version=1.0\n";
            out << "Name=whatsit\n";
            // out << "X-GNOME-Autostart-enabled=true\n"; // why only GNOME? anyway this is deprecated
            out << "Hidden=false\n"; // current standard; desktop agnostic
            out << "Categories=Network;Chat;\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=whatsit\n";
            out << "Terminal=false\n";
        }
    } else {
        QFile::remove(desktopFile); // removes only this file
    }
}
