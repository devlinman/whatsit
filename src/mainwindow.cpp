#include "mainwindow.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include <QStandardPaths>
#include <QDir>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
view(new QWebEngineView(this)),
profile(nullptr),
settings(QSettings::IniFormat,
         QSettings::UserScope,
         "whatsit",
         "whatsit")
{
    resize(1200, 800);
    setCentralWidget(view);

    setupWebEngine();
    view->load(QUrl("https://web.whatsapp.com"));
}

void MainWindow::setupWebEngine()
{
    const QString dataPath =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString cachePath =
    QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    QDir().mkpath(dataPath);
    QDir().mkpath(cachePath);

    profile = new QWebEngineProfile("whatsit-profile", this);

    // ✅ Persistent storage
    profile->setPersistentStoragePath(dataPath);
    profile->setCachePath(cachePath);
    profile->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);

    // ✅ Force modern Chromium User-Agent for WhatsApp Web compatibility
    profile->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36"
    );

    auto *page = new QWebEnginePage(profile, view);
    view->setPage(page);

    applyDarkModeIfNeeded();
}

void MainWindow::applyDarkModeIfNeeded()
{
    settings.beginGroup("General");
    const bool preferDark =
    settings.value("PreferDarkMode", true).toBool();
    settings.endGroup();

    if (!preferDark)
        return;

    QWebEngineScript script;
    script.setName("ForceWhatsAppDarkMode");
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);

    script.setSourceCode(R"JS(
        try {
            localStorage.setItem("theme", "dark");
            document.documentElement.classList.add("dark");
        } catch (e) {}
    )JS");

    profile->scripts()->insert(script);
}
