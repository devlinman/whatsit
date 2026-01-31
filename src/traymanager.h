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

signals:
    void showRequested();
    void hideRequested();
    void activated();

private:
    KStatusNotifierItem *tray;
};
