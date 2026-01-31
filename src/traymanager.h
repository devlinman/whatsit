// traymanager.h
#pragma once

#include <QObject>

class KStatusNotifierItem;

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(QObject *parent = nullptr);

    void initialize();
    void setIcon(const QString &iconName);
    void setUnreadIndicator(bool show);
    void setIndicatorEnabled(bool enabled);

signals:
    void showRequested();
    void hideRequested();
    void activated();

private:
    void updateIcon();

    KStatusNotifierItem *tray;
    QString m_currentIconName;
    bool m_showUnreadIndicator = false;
    bool m_indicatorEnabled = true;
};
