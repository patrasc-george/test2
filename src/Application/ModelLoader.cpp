#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QFile>
#include <QMessageBox>

#include <ObjectDetection.h>
#include "ModelLoader.h"

void ModelLoader::loadAll(QString path, QVector<Detector*>& vector) {
    // read the json file
    QString jsonText;
    QFile file(path);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    jsonText = file.readAll();
    file.close();

    // get the onjects
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
    QJsonArray objects = doc.array();

    //go through every object
    for (int i = 0; i < objects.size(); i++) {
        detectorProperties props;

        QJsonObject obj = objects[i].toObject();

        Detector* det;
        try {
            getFromJsonObject(det, obj);
            vector.push_back(det);
        }
        catch (std::exception& e) {
            qCritical(e.what());
            delete& det;
        }
    }
}

bool ModelLoader::getFromFileByName(Detector*& det, QString name, QString jsonPath) {
    QString jsonText;
    QFile file(jsonPath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    jsonText = file.readAll();
    file.close();

    // get the onjects
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
    QJsonArray objects = doc.array();

    QJsonValue currName;
    //go through every object
    for (int i = 0; i < objects.size(); i++) {
        QJsonObject obj = objects[i].toObject();
         currName = obj.value("name");
         if (currName == name) {
             return getFromJsonObject(det, obj);
         }
    }
    return false;
}

bool ModelLoader::getFromJsonObject(Detector*& det, QJsonObject obj) {
    detectorProperties props;

    QJsonValue value = obj.value("type");
    if (value == "network") {
        value = obj.value("properties");
        QJsonObject item = value.toObject();

        props.framework = item["framework"].toString().toStdString();
        props.shouldSwapRB = item["swapRB"].toBool();

        QJsonArray means = item["meanValues"].toArray();
        props.meanValues = cv::Scalar(means[0].toDouble(), means[1].toDouble(), means[2].toDouble());

        value = obj.value("paths");
        item = value.toObject();

        props.infGraphPath = item["inf"].toString().toStdString();
        props.classNamesPath = item["classes"].toString().toStdString();
        props.modelPath = item["model"].toString().toStdString();

        try {
            det = new ObjectDetector(props);
            return det->init();
        }
        catch (const std::exception& ex) {
            qCritical(ex.what());
            return false;
        }
    }
    else if (value == "cascade") {
        value = obj.value("paths");
        QJsonObject item = value.toObject();

        props.modelPath = item["face"].toString().toStdString();

        try {
            det = new FaceDetector(props, item["eyes"].toString().toStdString());
            return det->init();
        }
        catch (const std::exception& ex) {
            qCritical(ex.what());
            return false;
        }
    }
    return false;
}

QVector<QString> ModelLoader::getNames(QString jsonPath) {
    QString jsonText;
    QFile file(jsonPath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    jsonText = file.readAll();
    file.close();

    // get the onjects
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
    QJsonArray objects = doc.array();

    QJsonValue currName;
    QVector<QString> v;
    //go through every object
    for (int i = 0; i < objects.size(); i++) {
        QJsonObject obj = objects[i].toObject();
        currName = obj.value("name");
        if (currName.toString().isEmpty() == false)
            v.push_back(currName.toString());
    }
    return v;
}