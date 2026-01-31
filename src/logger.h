// logger.h
#pragma once

#include <QString>

class Logger {
public:
    static void log(const QString &message);
    static void setFileLoggingEnabled(bool enabled);
    static bool isFileLoggingEnabled();
    static void deleteLogFile();

private:
    static bool s_fileLoggingEnabled;
};
