#include <QCoreApplication>
#include <QtCore>
#include "qsettings_json.hpp"

int main(int argc, char *argv[])
{
    QSettings settings("config.json", JsonFormat);

    if (settings.status() == QSettings::AccessError){
        qDebug().noquote() << "Access error in" << settings.fileName();
        return 1;
    }
    else if (settings.status() == QSettings::FormatError){
        qDebug().noquote() << "Format error in" << settings.fileName();
        return 2;
    }

//    qDebug().noquote() << "File: " << settings.fileName() << Qt::endl;

//    setting.setValue("test1", QString("test1"));
//    setting.setValue("test1/test1.1", QString("test1.1"));
    settings.setValue("test1/test1.3", QString("v_test1.3"));

//    setting.beginGroup("test1");
//    qDebug().noquote() << setting.value("test1.1").toString();
//    setting.endGroup();

    // dump setting
    qDebug().noquote() << "Settings Dump";
    QStringList allKeys = settings.allKeys();
    for (const QString &key : qAsConst(allKeys)){
        qDebug().noquote() << key << ": " << settings.value(key);
    }

    settings.sync();

    return 0;
}
