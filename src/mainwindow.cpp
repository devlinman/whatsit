#include "mainwindow.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

#include <QStandardPaths>
#include <QDir>
#include <QUrl>

static constexpr int DEFAULT_WIDTH  = 1200;
static constexpr int DEFAULT_HEIGHT = 800;

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
view(new QWebEngineView(this)),
profile(nullptr),
settings(QSettings::IniFormat,
         QSettings::UserScope,
         "whatsit",
         "whatsit")
{
    setMinimumSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    initializeConfigDefaults();
    restoreWindowFromConfig();

    setCentralWidget(view);

    setupWebEngine();
    view->load(QUrl("https://web.whatsapp.com"));
}

void MainWindow::initializeConfigDefaults()
{
    // General section
    settings.beginGroup("General");
    if (!settings.contains("PreferDarkMode"))
        settings.setValue("PreferDarkMode", 0);
    settings.endGroup();

    // Window section
    settings.beginGroup("Window");
    if (!settings.contains("RememberPreviousWindowSize"))
        settings.setValue("RememberPreviousWindowSize", 0);
    if (!settings.contains("MaximizeByDefault"))
        settings.setValue("MaximizeByDefault", 0);
    settings.endGroup();

    settings.sync();
}

void MainWindow::restoreWindowFromConfig()
{
    settings.beginGroup("Window");

    const bool rememberSize =
    settings.value("RememberPreviousWindowSize").toBool();
    const bool maximizeByDefault =
    settings.value("MaximizeByDefault").toBool();

    const QSize storedSize =
    settings.value("Size", QSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)).toSize();

    settings.endGroup();

    if (maximizeByDefault) {
        showMaximized();
        return;
    }

    if (rememberSize) {
        resize(storedSize.expandedTo(
            QSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)));
    } else {
        resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    }
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

    profile->setPersistentStoragePath(dataPath);
    profile->setCachePath(cachePath);
    profile->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);

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
    settings.value("PreferDarkMode").toBool();
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowToConfig();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveWindowToConfig()
{
    settings.beginGroup("Window");

    const bool rememberSize =
    settings.value("RememberPreviousWindowSize").toBool();

    if (rememberSize && !isMaximized()) {
        settings.setValue("Size", size());
    }

    settings.endGroup();
    settings.sync();
}
