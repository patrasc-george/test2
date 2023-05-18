#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QFile>
#include <QMessageBox>

#include <ObjectDetection.h>
#include "ModelLoader.h"

/**
 * @brief Loads all detectors from a JSON file.
 * @param[in] path The path to the JSON file.
 * @param[out] vector The vector to store the loaded detectors.
 * @details This function reads a JSON file containing an array of detector objects. It then creates a Detector object for each object in the array and stores it in the provided vector. If an error occurs while creating a Detector object, it is logged using qCritical and the object is deleted.
 */
void ModelLoader::loadAll(const QString& path, QVector<Detector*>& vector) {
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

/**
 * @brief Gets a detector from a JSON file by its name.
 * @param[out] det The pointer to the detector to be loaded.
 * @param[in] name The name of the detector to be loaded.
 * @param[in] jsonPath The path to the JSON file.
 * @return Returns 0 if successful, otherwise returns an error code.
 * @details This function reads a JSON file containing an array of detector objects and searches for an object with the specified name. If found, it creates a Detector object from the JSON object and stores it in the provided pointer. If not found, it returns an error code indicating that the name was not found.
 */
int ModelLoader::getFromFileByName(Detector*& det, const QString& name, const QString& jsonPath) {
    QJsonObject obj = getObjectByName(name, jsonPath);
    if (!obj.empty())
        return getFromJsonObject(det, obj);

    return NAME_NOT_FOUND;
}

/**
 * @brief Gets a detector from a JSON object.
 * @param[out] det The pointer to the detector to be loaded.
 * @param[in] obj The JSON object containing the detector data.
 * @return Returns 0 if successful, otherwise returns an error code.
 * @details This function creates a Detector object from a JSON object. It checks the "type" field of the JSON object to determine what type of Detector to create (network or cascade) and then reads the relevant properties from the JSON object and uses them to initialize the Detector object. If successful, it stores the Detector object in the provided pointer. If not successful, it returns an error code indicating what went wrong (e.g. type not provided).
 */
int ModelLoader::getFromJsonObject(Detector*& det, const QJsonObject& obj) {
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

        det = new ObjectDetector(props);
        return det->init();
    }
    else if (value == "cascade") {
        value = obj.value("paths");
        QJsonObject item = value.toObject();

        props.modelPath = item["face"].toString().toStdString();

        det = new FaceDetector(props, item["eyes"].toString().toStdString(), item["smile"].toString().toStdString());
        det->currentClassName = obj.value("name").toString().toStdString();
        return det->init();
    }
    return TYPE_NOT_PROVIDED;
}

/**
 * @brief Gets the names of all detectors from a JSON file.
 * @param[in] jsonPath The path to the JSON file.
 * @return Returns a vector of detector names.
 * @details This function reads a JSON file containing an array of detector objects and extracts the "name" field from each object. It then stores all the names in a vector and returns it. If an object does not have a "name" field or if it is empty, it is skipped.
 */
QVector<QString> ModelLoader::getNames(const QString& jsonPath) {
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

/**
 * @brief Gets a JSON object representing a detector by its name.
 * @param[in] name The name of the detector to be loaded.
 * @param[in] jsonPath The path to the JSON file.
 * @return Returns the JSON object representing the detector if found, otherwise returns an empty JSON object.
 * @details This function reads a JSON file containing an array of detector objects and searches for an object with the specified name. If found, it returns the JSON object representing the detector. If not found, it returns an empty JSON object.
 */
QJsonObject ModelLoader::getObjectByName(const QString& name, const QString& jsonPath) {
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
        if (currName == name)
            return obj;
    }
    return QJsonObject();
}