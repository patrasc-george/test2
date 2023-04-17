#include "ObjectDetection.h"

#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>

void drawLabel(cv::Mat & image, std::string label, int left, int top) {
    int baseLine;
    cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.7, 1.5, &baseLine);

    top = std::max(top, label_size.height);
    cv::Point tlc = cv::Point(left, top);
    cv::Point brc = cv::Point(left + label_size.width, top + label_size.height + baseLine);
    rectangle(image, tlc, brc, cv::Scalar(147, 167, 255), cv::FILLED);
    putText(image, label, cv::Point(left, top + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 1.5);
}

FaceDetector::FaceDetector(detectorProperties& props, std::string eyeClassifierPath) {
    modelPath = props.modelPath;
    shouldSwapRB = props.shouldSwapRB;
    try {
        faceClassifier.load(modelPath);
}
    catch (const std::exception& ex) {
        std::cerr << "Couldn't load face classifier!" << std::endl;
    }

    if (eyeClassifierPath != "\0") {
        try {
            eyeClassifier.load(eyeClassifierPath);
        }
        catch (const std::exception&) {
            std::cerr << "Couldn't load eye classifier!" << std::endl;
        }
    }
}

void FaceDetector::detect(cv::Mat& image, bool showEyes) {
    faceClassifier.detectMultiScale(image, facesInFrame);
    if (facesInFrame.size() == 0)
        return;
    for (auto&& face : facesInFrame) {
        rectangle(image, face, cv::Scalar(147, 167, 255), 2);
        drawLabel(image, "Face", face.x, face.y);
        if (showEyes == false)
            break;

        // The area of interest in the image is obtained
        cv::Mat faceROI = image(face);

        // In each face, detect eyes
        std::vector<cv::Rect> eyes;
        eyeClassifier.detectMultiScale(faceROI, eyes);

        for (int j = 0; j < eyes.size(); j++) {
            cv::Point eye_center(face.x + eyes[j].x + eyes[j].width / 2, face.y + eyes[j].y + eyes[j].height / 2);
            int radius = cvRound((eyes[j].width + eyes[j].height) * 0.25);
            circle(image, eye_center, radius, cv::Scalar(239, 190, 98), 3);
        }
    }
}

ObjectDetector::ObjectDetector(detectorProperties props) {
    modelPath = props.modelPath;
    classNamesPath = props.classNamesPath;
    infGraphPath = props.infGraphPath;
    framework = props.framework;
    shouldSwapRB = props.shouldSwapRB;
    meanValues = props.meanValues;

    try {
        std::ifstream ifs(classNamesPath);
        std::string line;
        while (getline(ifs, line))
        {
            classNames.push_back(line);
        }
    }
    catch (const std::exception& ex) {
        throw ex;
    }
    try {
        model = cv::dnn::readNet(infGraphPath, modelPath, framework);
    }
    catch (const std::exception& ex) {
        throw ex;
    }
} 

void ObjectDetector::detect(cv::Mat& image, bool detectEyes) { // 'detectEyes' is by default false, setting it to true does nothing when detecting *objects*
    cv::Mat blob = cv::dnn::blobFromImage(image, 1.0, cv::Size(224,224), meanValues, shouldSwapRB, false);

    model.setInput(blob);
    cv::Mat output = model.forward();

    cv::Mat detectionMat(output.size[2], output.size[3], CV_32F, output.ptr<float>());

    for (int i = 0; i < detectionMat.rows; i++) {
        int classId = detectionMat.at<float>(i, 1);
        float confidence = detectionMat.at<float>(i, 2);

        if (confidence > 0.7) {
            int box_x = (int) (detectionMat.at<float>(i, 3) * image.cols);
            int box_y = (int) (detectionMat.at<float>(i, 4) * image.rows);
            int box_width = (int) (detectionMat.at<float>(i, 5) * image.cols - box_x);
            int box_height = (int) (detectionMat.at<float>(i, 6) * image.rows - box_y);
            cv::rectangle(image, cv::Point(box_x, box_y), cv::Point(box_x + box_width, box_y + box_height), cv::Scalar(147, 167, 255), 2);
            drawLabel(image, classNames[classId - 1].c_str() + std::string(": confidence = ") + std::to_string(confidence), box_x, box_y);
        }
    }
}