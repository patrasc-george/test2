#include "ObjectDetector.h"

#include <algorithm>
cv::Rect ObjectDetector::getLastRect() {
	return lastRect;
}

ObjectDetector::ObjectDetector(detectorProperties props) {
	modelPath = props.modelPath;
	classNamesPath = props.classNamesPath;
	infGraphPath = props.infGraphPath;
	framework = props.framework;
	shouldSwapRB = props.shouldSwapRB;
	meanValues = props.meanValues;
	type = network;
}

int ObjectDetector::init() {
	if (modelPath == "\0")
		return -2;
	if (infGraphPath == "\0")
		return -4;
	std::ifstream ifs(classNamesPath);
	std::string line;
	while (getline(ifs, line))
		classNames.push_back(std::make_pair(line, false));
	try {
		model = cv::dnn::readNet(infGraphPath, modelPath, framework);
	}
	catch (cv::Exception&) {
		return -5;
	}
	return 1;
}

void ObjectDetector::setClassNamesValues(const std::vector<bool> classesToShow)
{
	for (int i = 0; i < classNames.size(); i++)
		classNames[i].second = classesToShow.at(i);
}

void ObjectDetector::sort(const cv::Mat& detectionMat)
{
	sortedClassNames.clear();
	std::map<int, float> counter;
	for (int i = 0; i < detectionMat.rows; i++)
	{
		int classId = detectionMat.at<float>(i, 1) - 1;
		counter[classId]++;
	}
	for (const auto& pair : counter)
		sortedClassNames.push_back(classNames[pair.first].first);
	for (const auto& className : classNames)
		if (std::find(sortedClassNames.begin(), sortedClassNames.end(), className.first) == sortedClassNames.end())
			sortedClassNames.push_back(className.first);
}

std::vector<std::string> ObjectDetector::getSortedClassNames() const
{
	return sortedClassNames;
}

void ObjectDetector::detect(cv::Mat& image, bool showConf) {
	if (image.type() == CV_8UC4)
		cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
	cv::Mat blob = cv::dnn::blobFromImage(image, 1.0, cv::Size(320, 320), meanValues, shouldSwapRB, false);
	std::vector<std::string> layers = model.getLayerNames();

	model.setInput(blob);
	try {
		/*
		In Debug mode, the forward method requires a string argument representing the layer,
		otherwise it throws a fatal exception and the whole app crashes.
		In Release mode, it automatically chooses the right layer
		*/
#ifdef NDEBUG
		blob = model.forward();
#else
		blob = model.forward("layer");
#endif
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		std::exception ex("No valid layer was provided to model.forward(). This would happen if the application is run in Debug mode.");
		throw ex;
		return;
	}

	cv::Mat detectionMat(blob.size[2], blob.size[3], CV_32F, blob.ptr<float>());
	sort(detectionMat);

	for (int i = 0; i < detectionMat.rows; i++) {
		int classId = detectionMat.at<float>(i, 1);
		float confidence = detectionMat.at<float>(i, 2);

		if (confidence > minConfidence && classNames[classId - 1].second) {
			int box_x = (int)(detectionMat.at<float>(i, 3) * image.cols);
			int box_y = (int)(detectionMat.at<float>(i, 4) * image.rows);
			int box_width = (int)(detectionMat.at<float>(i, 5) * image.cols - box_x);
			int box_height = (int)(detectionMat.at<float>(i, 6) * image.rows - box_y);
			lastRect = cv::Rect(box_x, box_y, box_width, box_height);

			currentClassName = classNames[classId - 1].first.c_str();
			std::stringstream ss;
			ss << currentClassName;
			if (showConf)
				ss << ": confidence = " + std::to_string((int)(confidence * 100)) + "%";
			drawLabel(image, ss.str(), lastRect);
		}
	}
}

void ObjectDetector::setMinConfidence(float c) {
	if (c > 0 && c < 1)
		minConfidence = c;
}
