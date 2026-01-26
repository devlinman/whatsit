// traymanager.cpp
#include "traymanager.h"
#include <QMenu>

TrayManager::TrayManager(QObject *parent)
: QObject(parent),
tray(nullptr)
{
}

void TrayManager::initialize()
{
    tray = new KStatusNotifierItem(this);
    tray->setTitle("whatsit");
    tray->setCategory(KStatusNotifierItem::ApplicationStatus);
    tray->setStatus(KStatusNotifierItem::Active);
    tray->setIconByName("whatsit");

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
    tray->setIconByName(iconName.isEmpty() ? "whatsit" : iconName);
}
