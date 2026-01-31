// webenginehelper.cpp
#include "webenginehelper.h"
#include "configmanager.h"
#include "logger.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEnginePermission>
#include <QWebEngineNotification>
#include <KNotification>
#include <QWebEngineDownloadRequest>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

namespace {

    const QString DEFAULT_USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) "
                                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                                       "Chrome/120.0.0.0 Safari/537.36";

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

}

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

    m_profile->setHttpUserAgent(DEFAULT_USER_AGENT);

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

        KNotification *knotify = new KNotification("whatsapp-message", KNotification::CloseOnTimeout);
        knotify->setComponentName("whatsit");
        knotify->setHint("desktop-entry", "whatsit");
        knotify->setTitle(notification->title());
        
        QString message = notification->message();
        if (message.isEmpty()) {
            message = "New Message";
            Logger::log("Message was empty, using fallback.");
        }
        knotify->setText(message);
        knotify->setIconName("whatsit");

        QImage icon = notification->icon();
        if (!icon.isNull()) {
            Logger::log("Notify::Icon: Valid (" + QString::number(icon.width()) + "x" + QString::number(icon.height()) + ")");
            knotify->setPixmap(QPixmap::fromImage(icon));
        } else {
            Logger::log("Notify::Icon: Null/Empty");
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
            Logger::log("Origin: " + permission.origin().toString());
            Logger::log("Granting permission...");
            permission.grant();
            Logger::log("Permission granted.");
        } else {
             Logger::log("WebEngineHelper: Unknown Permission Requested");
        }
    });

    setAudioMuted(m_config->muteAudio());
}

void WebEngineHelper::setAudioMuted(bool muted)
{
    Logger::log(QString("WebEngineHelper: Setting audio muted to %1").arg(muted));
    if (m_view && m_view->page()) {
        m_view->page()->setAudioMuted(muted);
    }
}

void WebEngineHelper::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    // Debug: log download file name. disabled for privacy.
    // Logger::log("Download requested: " + download->suggestedFileName());
    Logger::log("Download requested");
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

    if (filePath.isEmpty()) {
        Logger::log("File path is empty. Cancelling download");
        return;
    }
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
