// traymanager.cpp
#include "traymanager.h"
#include <KStatusNotifierItem>
#include <QMenu>
#include <QPainter>
#include <QIcon>
#include <QPixmap>

TrayManager::TrayManager(QObject *parent)
: QObject(parent),
tray(nullptr),
m_currentIconName("whatsit")
{
}

void TrayManager::initialize()
{
    tray = new KStatusNotifierItem(this);
    tray->setTitle("whatsit");
    tray->setCategory(KStatusNotifierItem::ApplicationStatus);
    tray->setStatus(KStatusNotifierItem::Active);
    
    updateIcon();

    auto *menu = new QMenu;
    menu->addAction(QIcon::fromTheme("view-visible"), "Show", this, &TrayManager::showRequested);
    menu->addAction(QIcon::fromTheme("view-hidden"), "Hide", this, &TrayManager::hideRequested);

    tray->setContextMenu(menu);

    // Left-click on tray icon
    connect(tray, &KStatusNotifierItem::activateRequested,
            this, &TrayManager::activated);
}

void TrayManager::setIcon(const QString &iconName)
{
    m_currentIconName = iconName.isEmpty() ? "whatsit" : iconName;
    updateIcon();
}

void TrayManager::setUnreadIndicator(bool show)
{
    if (m_showUnreadIndicator == show)
        return;

    m_showUnreadIndicator = show;
    updateIcon();
}

void TrayManager::setIndicatorEnabled(bool enabled)
{
    if (m_indicatorEnabled == enabled)
        return;

    m_indicatorEnabled = enabled;
    updateIcon();
}

void TrayManager::updateIcon()
{
    if (!tray) return;

    QIcon icon = QIcon::fromTheme(m_currentIconName);
    if (icon.isNull())
        icon = QIcon(m_currentIconName);

    if (!m_showUnreadIndicator || !m_indicatorEnabled) {
        tray->setIconByPixmap(icon);
        return;
    }

    // Draw red dot
    QPixmap pixmap = icon.pixmap(64, 64);
    if (pixmap.isNull()) {
        pixmap = QPixmap(64, 64);
        pixmap.fill(Qt::transparent);
    }

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::red);
    painter.setPen(Qt::NoPen);

    // Increase dot size by 50% (from 1/4 to 3/8)
    int dotSize = (pixmap.width() * 3) / 8;
    painter.drawEllipse(pixmap.width() - dotSize - 2, 2, dotSize, dotSize);
    painter.end();

    tray->setIconByPixmap(QIcon(pixmap));
}
