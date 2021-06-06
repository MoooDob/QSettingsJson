#ifndef QSETTINGS_JSON_HPP
#define QSETTINGS_JSON_HPP

#include <QtCore>
#include <QObject>
#include <QDebug>

bool readSettingsJson(QIODevice &device, QSettings::SettingsMap &map);
bool writeSettingsJson(QIODevice &device, const QSettings::SettingsMap &map);
bool parseJsonValue(const QString &jsonKey, const QJsonValue &jsonValue, QString prefix, QSettings::SettingsMap &map);
bool parseJsonArray(const QJsonArray &jsonArray, QString prefix, QSettings::SettingsMap &map);
QJsonValue restoreJsonValue (QSettings::SettingsMap &map, QStringList &keysWL, QStringList &sections, int sectionLevel, QVariant &value);



static const QSettings::Format JsonFormat = QSettings::registerFormat("JsonFormat", &readSettingsJson, &writeSettingsJson);




// -------------------- Reading -------------------------


bool parseJsonArray(const QJsonArray &jsonArray, QString prefix, QSettings::SettingsMap &map){
    int arr_ctr=0;
    if (! jsonArray.isEmpty()) {

        for (const QJsonValue &arrayItem: jsonArray){
            QString arrItemkey = QString("#%1").arg(arr_ctr, /*fieldWidth=*/ 5, /*base=*/ 10, /*fillChar=*/ QChar('0'));
            if (parseJsonValue(arrItemkey, arrayItem, prefix, map))
                arr_ctr++;
        }
        return true;
    } else
        // empty array
        qWarning().noquote() << "empty JsonArray" << prefix << "ignored during read, some array item numbering will possibly be changed skipped.";
        return false;
};


bool parseJsonObject(QJsonObject &jsonObject, QString prefix, QSettings::SettingsMap &map)
{
    QStringList keys = jsonObject.keys();
    if (! keys.isEmpty()) {
        for(int i=0; i<keys.size(); i++) {
            QString currentKey = keys[i];
            QJsonValue currentValue = jsonObject.value(currentKey);
            parseJsonValue(currentKey, currentValue, prefix, map);
        }
        return true;
    } else
        // empty object
        qWarning().noquote() << "empty JsonObject" << prefix << "ignored during read, some array item numbering will possibly be changed skipped.";
        return false;
}


bool parseJsonValue(const QString &jsonKey, const QJsonValue &jsonValue, QString prefix, QSettings::SettingsMap &map)
{

    if(jsonValue.isObject())
    {
        QJsonObject jsonObj = jsonValue.toObject();
        return parseJsonObject(jsonObj, prefix + jsonKey + "/", map);
    }
    else if(jsonValue.isArray()){
        QJsonArray jsonArray = jsonValue.toArray();
        return parseJsonArray(jsonArray, prefix + jsonKey + "/", map);
    }
    else if(jsonValue.isUndefined()){
        map.insert(prefix + jsonKey, QVariant(QVariant::Invalid));
    }
    else
    {
        map.insert(prefix + jsonKey, jsonValue.toVariant());
    }
    return true;
}

bool readSettingsJson(QIODevice &device, QSettings::SettingsMap &map)
{
//    if (dynamic_cast<QFile*>(&device)){
//        QFile* f = dynamic_cast<QFile*>(&device);
//        qDebug() << "Filename: " << f->fileName();
//    }

    QJsonParseError jsonError;
    QString jsonString = device.readAll();
//    qWarning() << jsonString;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &jsonError);

    if(jsonError.error != QJsonParseError::NoError)
        return false;

    if (jsonDoc.isObject()) {
        QJsonObject jsonObject = jsonDoc.object();
         parseJsonObject(jsonObject, QString(), map);
    } else {
        QJsonArray jsonArray = jsonDoc.array();
        parseJsonArray(jsonArray, QString(), map);

    }
    return true;
}



// -------------------- Writing -------------------------


QJsonValue restoreJsonSimpleValue(QSettings::SettingsMap &map, QStringList &keysWL, QStringList &sections, QVariant &value)
{
    QJsonValue jsonValue;

    if (value.isNull()) {
        jsonValue = QJsonValue();
    } else {
        jsonValue = QJsonValue::fromVariant(value);
    }

    keysWL.removeFirst();

    if (!keysWL.isEmpty()){
        QString newKey = keysWL.first();
        sections = newKey.split('/');
        value = map[newKey];
    } else {
        sections = QStringList();
        value = QVariant(QVariant::Invalid);
    }
    return jsonValue;
}

QJsonObject restoreJsonObject(QSettings::SettingsMap &map, QStringList &keysWL, QStringList &sections, int sectionLevel, QVariant &value)
{
    QJsonObject jsonObject;

    const QStringList path = sections.mid(0, sectionLevel);

    while (! keysWL.isEmpty()) {
        const QString key = sections[sectionLevel];
        jsonObject.insert(key, restoreJsonValue(map, keysWL, sections, sectionLevel + 1, value));
        if (keysWL.isEmpty() ||
                path.size() > sections.size() ||
                path != sections.mid(0, sectionLevel)
                ) {
            break;
        }
    }

    return jsonObject;

}


int getArrayIndex(QString section, bool *ok = nullptr) {
    *ok = false;
    int index = -1;
    if (section.startsWith("#")) {
        // check for Array
        // strip # and convert to int
        index = section.midRef(1).toInt(ok);
    }
    return index;
}


QJsonArray restoreJsonArray(QSettings::SettingsMap &map, QStringList &keysWL, QStringList &sections, int sectionLevel, QVariant &value)
{
    QJsonArray jsonArray;

    if (sectionLevel >= sections.size()) {
        qWarning().noquote() << "Structural error.";
        return jsonArray;
    }

    const QStringList path = sections.mid(0, sectionLevel);

    while (!keysWL.isEmpty()) {
        const QString arrayIndexSection = sections.at(sectionLevel);
        bool conversion_result_ok = false;
        getArrayIndex(arrayIndexSection, &conversion_result_ok);
        if (conversion_result_ok) {
            QJsonValue jsonValue = restoreJsonValue(map, keysWL, sections, sectionLevel + 1, value);
            jsonArray.append(jsonValue);
            if (keysWL.isEmpty() ||
                    path.size() >= sections.size() ||
                    path != sections.mid(0, sectionLevel)
                    ) {
                break;
            }
        } else {
            qWarning().noquote() << "Array index expected, got" << arrayIndexSection << ".";
        }
    }

    return jsonArray;
}


QJsonValue restoreJsonValue (QSettings::SettingsMap &map, QStringList &keysWL, QStringList &sections, int sectionLevel, QVariant &value) {

    int section_size = sections.size();

    if (section_size <= sectionLevel) {
        // is last section => SimpleValue and last section is key
        return restoreJsonSimpleValue(map, keysWL, sections, value);
    }

    // Object or Array
    if (sectionLevel < sections.size()) {
        const QString arrayIndexSection = sections.at(sectionLevel);
        bool conversion_result_ok = false;
        getArrayIndex(arrayIndexSection, &conversion_result_ok);
        if (conversion_result_ok) {
            // is Array
            return restoreJsonArray(map, keysWL, sections, sectionLevel, value);
        }
    }

    // Object
    return restoreJsonObject(map, keysWL, sections, sectionLevel, value);

}




bool writeSettingsJson(QIODevice &device, const QSettings::SettingsMap &map)
{
    QJsonValue buffer;
    QMap<QString, QVariant> tmp_map = map;
    QStringList keysWL = map.keys();
    if (! keysWL.isEmpty()) {
        QStringList sections = keysWL.first().split('/');
        int sectionLevel = 0;
        QVariant value = map[keysWL.first()];
        buffer = restoreJsonValue(tmp_map, keysWL, sections, sectionLevel, value);;
    }
    if (buffer.type() == QJsonValue::Object) {
        device.write(QJsonDocument(buffer.toObject()).toJson());
    } else if (buffer.type() == QJsonValue::Array) {
        device.write(QJsonDocument(buffer.toArray()).toJson());
    } else {
            device.write(QJsonDocument().toJson());
    }
    return true;
}
#endif // QSETTINGS_JSON_HPP
