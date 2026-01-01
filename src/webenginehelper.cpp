// webenginehelper.cpp
#include "webenginehelper.h"
#include "configmanager.h"
#include "logger.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEnginePermission>
#include <QWebEngineNotification>
#include <KNotification>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineDownloadRequest>
#include <QWebChannel>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>

namespace {

    class WhatsitPage : public QWebEnginePage
    {
    public:
        explicit WhatsitPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent),
        m_profile(profile)
        {}

    protected:
        void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                      const QString &message,
                                      int lineNumber,
                                      const QString &sourceID) override
        {
            if (message.contains("Error with Permissions-Policy header") ||
                message.contains("multiple-uim-roots") ||
                message.contains("Subsequent non-fatal errors won't be logged")) {
                return;
            }
            Logger::log(QString("JS Console: %1 (Line %2, Source: %3)").arg(message).arg(lineNumber).arg(sourceID));
            QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
        }

        bool acceptNavigationRequest(const QUrl &url,
                                     NavigationType type,
                                     bool) override
                                     {
                                         if (type == QWebEnginePage::NavigationTypeLinkClicked &&
                                             !url.toString().startsWith("https://web.whatsapp.com")) {
                                             QDesktopServices::openUrl(url);
                                         return false;
                                             }
                                             return true;
                                     }

                                     QWebEnginePage *createWindow(WebWindowType) override
                                     {
                                         return new ExternalPage(m_profile, this);
                                     }

    private:
        QWebEngineProfile *m_profile;

        class ExternalPage : public QWebEnginePage
        {
        public:
            explicit ExternalPage(QWebEngineProfile *profile, QObject *parent = nullptr)
            : QWebEnginePage(profile, parent)
            {}

        protected:
            bool acceptNavigationRequest(const QUrl &url,
                                         NavigationType,
                                         bool) override
                                         {
                                             QDesktopServices::openUrl(url);
                                             return false;
                                         }
        };
    };

} // namespace

WebEngineHelper::WebEngineHelper(QWebEngineView *view,
                                 ConfigManager *config,
                                 QObject *parent)
: QObject(parent),
m_view(view),
m_profile(nullptr),
m_config(config)
{
}

void WebEngineHelper::initialize()
{
    Logger::log("WebEngineHelper::initialize");
    const QString dataPath =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString cachePath =
    QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    QDir().mkpath(dataPath);
    QDir().mkpath(cachePath);

    m_profile = new QWebEngineProfile("whatsit-profile", this);

    m_profile->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36"
    );

    m_profile->setPersistentStoragePath(dataPath);
    m_profile->setCachePath(cachePath);
    m_profile->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);

    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, &WebEngineHelper::handleDownloadRequested);

    // Notification Presenter
    m_profile->setNotificationPresenter([this](std::unique_ptr<QWebEngineNotification> notification) {
        if (!m_config->systemNotifications()) {
            Logger::log("WebEngineHelper: Notification received but System Notifications are DISABLED.");
            return;
        }

        Logger::log("WebEngineHelper: New notification received.");
        // Logger::log("  Title: " + notification->title());
        // Logger::log("  Message: " + notification->message());
        // Logger::log("  Tag: " + notification->tag());

        KNotification *knotify = new KNotification("whatsapp-message", KNotification::CloseOnTimeout);
        knotify->setComponentName("whatsit");
        knotify->setHint("desktop-entry", "whatsit");
        knotify->setTitle(notification->title());
        
        QString message = notification->message();
        if (message.isEmpty()) {
            message = "New Message";
            Logger::log("  Message was empty, using fallback.");
        }
        knotify->setText(message);
        knotify->setIconName("whatsit");

        QImage icon = notification->icon();
        if (!icon.isNull()) {
            Logger::log("  Icon: Valid (" + QString::number(icon.width()) + "x" + QString::number(icon.height()) + ")");
            knotify->setPixmap(QPixmap::fromImage(icon));
        } else {
            Logger::log("  Icon: Null/Empty");
        }

        QWebEngineNotification *rawNotif = notification.release();
        rawNotif->setParent(knotify);

        auto *defaultAction = knotify->addDefaultAction(QString());
        
        // Handle click: Activate window AND tell WebEngine
        QObject::connect(defaultAction, &KNotificationAction::activated, rawNotif, [this, rawNotif]() {
            Logger::log("WebEngineHelper: Notification clicked. Activating window.");
            if (m_view && m_view->window()) {
                QWidget *win = m_view->window();
                if (win->isMinimized()) {
                    win->showNormal();
                } else {
                    win->show();
                }
                win->raise();
                win->activateWindow();
            }
            rawNotif->click();
        });

        QObject::connect(knotify, &KNotification::closed, rawNotif, &QWebEngineNotification::close);
        QObject::connect(rawNotif, &QWebEngineNotification::closed, knotify, &KNotification::close);
        QObject::connect(knotify, &KNotification::closed, knotify, &QObject::deleteLater);

        Logger::log("WebEngineHelper: Calling notification->show() and sending KNotification event...");
        rawNotif->show();
        knotify->sendEvent();
        Logger::log("WebEngineHelper: Notification displayed.");
    });

    auto *page = new WhatsitPage(m_profile, m_view);
    m_view->setPage(page);

    connect(page, &QWebEnginePage::permissionRequested,
            page, [page](QWebEnginePermission permission) {
        if (permission.permissionType() == QWebEnginePermission::PermissionType::Notifications) {
            Logger::log("WebEngineHelper: Notification Permission Requested via QWebEnginePermission");
            Logger::log("  Origin: " + permission.origin().toString());
            Logger::log("  Granting permission...");
            permission.grant();
            Logger::log("  Permission granted.");
        } else {
             Logger::log("WebEngineHelper: Unknown Permission Requested");
        }
    });

    applyTheme();
}

void WebEngineHelper::applyTheme()
{
    if (!m_profile || !m_view) {
        Logger::log("WebEngineHelper::applyTheme: Profile or View is null. Aborting.");
        return;
    }

    QWebEngineScript script;
    script.setName("whatsit_debug_injection");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(R"(
        console.log("WHATSIT_DEBUG: Injecting Notification Debugger...");
        const OriginalNotification = window.Notification;
        
        window.Notification = function(title, options) {
            console.log("WHATSIT_DEBUG: new Notification() called!");
            console.log("  Title:", title);
            console.log("  Options:", JSON.stringify(options));
            return new OriginalNotification(title, options);
        };
        
        // Copy static properties/methods
        for (let prop in OriginalNotification) {
            window.Notification[prop] = OriginalNotification[prop];
        }
        
        window.Notification.prototype = OriginalNotification.prototype;
        
        window.Notification.requestPermission = function(callback) {
             console.log("WHATSIT_DEBUG: Notification.requestPermission called!");
             return OriginalNotification.requestPermission(callback);
        };
        
        console.log("WHATSIT_DEBUG: Notification Debugger Injected.");
    )");
    m_profile->scripts()->insert(script);

    QWebEngineScript themeScript;
    themeScript.setName("whatsit_theme_injection");
    themeScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    themeScript.setWorldId(QWebEngineScript::MainWorld);
    themeScript.setRunsOnSubFrames(false);

    QString js;

    // Common helper function to be injected
    QString commonJs = R"JS(
        function safeSet(cls, mode) {
            try {
                if (!document.body) return;
                
                if (mode === 'dark') {
                    if (!document.body.classList.contains('dark')) {
                         document.body.classList.add('dark');
                    }
                    if (!document.body.classList.contains('web')) {
                         document.body.classList.add('web');
                    }
                    document.body.setAttribute('data-theme', 'dark');
                    try { localStorage.setItem('theme', '"dark"'); } catch(e) {}
                } else {
                    if (document.body.classList.contains('dark')) {
                        document.body.classList.remove('dark');
                    }
                    document.body.setAttribute('data-theme', 'light');
                    try { localStorage.setItem('theme', '"light"'); } catch(e) {}
                }
            } catch (e) {
                // Silenced to prevent log spam
            }
        }
    )JS";

    if (m_config->preferDarkMode()) {
        js = commonJs + R"JS(
            (function() {
                function enforce() { safeSet('dark', 'dark'); }
                
                if (document.readyState === 'loading') {
                    document.addEventListener('DOMContentLoaded', enforce);
                } else {
                    enforce();
                }

                // Robust Observer
                try {
                    const observer = new MutationObserver((mutations) => {
                        // Debounce or simple check to avoid infinite loops if the site fights back hard
                        // We simply check if the state is wrong before acting.
                        if (document.body && !document.body.classList.contains('dark')) {
                             enforce();
                        }
                    });
                    
                    if (document.body) {
                        observer.observe(document.body, { attributes: true, attributeFilter: ['class'] });
                    } else {
                        document.addEventListener('DOMContentLoaded', () => {
                             if(document.body) observer.observe(document.body, { attributes: true, attributeFilter: ['class'] });
                        });
                    }
                } catch(e) { console.error("Observer Error:", e); }
            })();
        )JS";
    } else {
        js = commonJs + R"JS(
            (function() {
                function enforce() { safeSet('dark', 'light'); }
                
                if (document.readyState === 'loading') {
                    document.addEventListener('DOMContentLoaded', enforce);
                } else {
                    enforce();
                }
                
                // Observer for light mode too, to prevent WA from auto-switching to dark if system is dark but user wants light here
                 try {
                    const observer = new MutationObserver((mutations) => {
                        if (document.body && document.body.classList.contains('dark')) {
                             enforce();
                        }
                    });
                    
                    if (document.body) {
                        observer.observe(document.body, { attributes: true, attributeFilter: ['class'] });
                    } else {
                         document.addEventListener('DOMContentLoaded', () => {
                             if(document.body) observer.observe(document.body, { attributes: true, attributeFilter: ['class'] });
                        });
                    }
                } catch(e) { console.error("Observer Error:", e); }
            })();
        )JS";
    }

    script.setSourceCode(js);
    m_profile->scripts()->insert(script);
    
    // Also run immediately if possible
    if(m_view->page()) {
        m_view->page()->runJavaScript(js);
    }
}

void WebEngineHelper::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    Logger::log("Download requested: " + download->suggestedFileName());
    QString baseDir = m_config->downloadPath();
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(
            QStandardPaths::DownloadLocation);
    }

    const QString suggested =
    QDir(baseDir).filePath(download->suggestedFileName());

    const QString filePath = QFileDialog::getSaveFileName(
        m_view,
        tr("Save File"),
            suggested
    );

    if (filePath.isEmpty())
        return;

    download->setDownloadFileName(QFileInfo(filePath).fileName());
    download->setDownloadDirectory(QFileInfo(filePath).absolutePath());
    download->accept();

    if (m_config->rememberDownloadPaths()) {
        m_config->setDownloadPath(QFileInfo(filePath).absolutePath());
        m_config->sync();
    }
}

QWebEngineProfile *WebEngineHelper::profile() const
{
    return m_profile;
}
