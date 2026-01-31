// logger.cpp
#include "logger.h"

#include <iostream>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

bool Logger::s_fileLoggingEnabled = false;

void Logger::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss");
    QString logMsg = QString("[%1] %2").arg(timestamp, message);

    std::cout << logMsg.toStdString() << std::endl;

    if (s_fileLoggingEnabled) {
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/whatsit";
        QDir().mkpath(cacheDir);
        QString logPath = cacheDir + "/whatsit.log";

        QFile file(logPath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << logMsg << "\n";
            file.close();
        }
    }
}

void Logger::setFileLoggingEnabled(bool enabled)
{
    s_fileLoggingEnabled = enabled;
}

bool Logger::isFileLoggingEnabled()
{
    return s_fileLoggingEnabled;
}

void Logger::deleteLogFile()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/whatsit";
    QString logPath = cacheDir + "/whatsit.log";
    
    QFile file(logPath);
    if (file.exists()) {
        std::cout << "Deleting log file: " << logPath.toStdString() << std::endl;
        if (file.remove()) {
             std::cout << "Log file deleted successfully." << std::endl;
        } else {
             std::cout << "Failed to delete log file." << std::endl;
        }
    }
}

