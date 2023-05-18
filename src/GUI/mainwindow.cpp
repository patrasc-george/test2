﻿#include "mainwindow.h"
#include "menuOptions.h"

#include <opencv2/opencv.hpp>

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QThread>
#include <QListWidget>
#include <QMessageBox>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStatusBar>
#include <QStandardPaths>
#include <QJsonObject>

#include "ImageProcessingUtils.h"
#include "../Application/ModelLoader.h"

#define modelsJSON "../data/detectors.json"
QVector<QString> names = ModelLoader::getNames(modelsJSON);

/**
 * @brief Constructs a MainWindow object.
 * @param[in] parent The parent widget of the MainWindow object.
 * @details This constructor initializes a MainWindow object with the provided parent widget. It creates and initializes various components such as a menu, image container, and status bar. These components are used to interact with the application and control its behavior. The constructor also connects signals emitted by the menu controls to slots in the MainWindow object that handle those signals. The components are arranged in a layout and added to the MainWindow widget.
 */
MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
	// instantiate the list of detectors
	this->currDet = nullptr;

	// instantiate the menu (see constructor), image container and status bar
	menu = new Menu;
	imageContainer = new SceneImageViewer;
	statusBar = new QStatusBar();
	resLabel = new QLabel();
	fpsLabel = new QLabel();
	statusBar->addPermanentWidget(resLabel);
	statusBar->addPermanentWidget(fpsLabel);
	statusBar->setSizeGripEnabled(false);
	statusBar->setFixedHeight(35);
	statusBar->setContentsMargins(0, 0, 20, 0);

	// link the controls defined in our menu to the events of our window
	// syntax: connect(widget that emits a signal, the type of the signal, the object that acts on the signal, the method (slot) that will be called)
	connect(menu->toggleCamera, &QAbstractButton::toggled, this, &MainWindow::toggleCameraEvent);
	connect(menu->uploadButton, &QPushButton::clicked, this, &MainWindow::uploadImageEvent);
	connect(menu->detectorsList, &QComboBox::currentIndexChanged, this, &MainWindow::selectDetectorEvent);
	connect(menu->screenshot, &QPushButton::clicked, this, &MainWindow::screenshotEvent);

	connect(menu->toggleFaceFeatures, &QCheckBox::clicked, this, [&] {
		history.add(SHOW_FEATURES, menu->toggleFaceFeatures->isChecked());
		statusBar->showMessage(QString("Toggled face features %1").arg(menu->toggleFaceFeatures->isChecked() ? "on" : "off"));
		processImage();
		});
	connect(menu->showConfidence, &QCheckBox::clicked, this, [&] {
		history.add(SHOW_CONFIDENCE, menu->showConfidence->isChecked());
		statusBar->showMessage(QString("Toggled show confidences %1").arg(menu->toggleFaceFeatures->isChecked() ? "on" : "off"));
		processImage();
		});

	connect(menu->confControl, &LabeledSlider::valueChanged, this, &MainWindow::changeMinConfEvent);
	connect(menu->thresholdControl, &LabeledSlider::valueChanged, this, &MainWindow::changeThresholdEvent);

	connect(menu->histogramEqualizationButton, &QPushButton::clicked, this, [&] {
		history.add(HISTOGRAM_EQUALIZATION, menu->histogramEqualizationButton->isChecked());
		processImage();
		});

	connect(menu->binaryThresholdingButton, &QPushButton::clicked, this, [&] {
		if (menu->binaryThresholdingButton->isChecked())
			history.add(BINARY_THRESHOLDING, menu->thresholdControl->value());
		else
			history.add(BINARY_THRESHOLDING, 0);
		processImage();
		});

	connect(menu->adaptiveThresholdingButton, &QPushButton::clicked, this, [&] {
		if (menu->adaptiveThresholdingButton->isChecked())
			history.add(ADAPTIVE_THRESHOLDING, menu->thresholdControl->value());
		else
			history.add(ADAPTIVE_THRESHOLDING, 0);
		processImage();
		});

	connect(menu->zeroThresholdingButton, &QPushButton::clicked, this, [&] {
		if (menu->zeroThresholdingButton->isChecked())
			history.add(ZERO_THRESHOLDING, menu->thresholdControl->value());
		else
			history.add(ZERO_THRESHOLDING, 0);
		processImage();
		});

	connect(menu->detectEdgesButton, &QPushButton::clicked, this, [&] {
		history.add(DETECT_EDGES, menu->detectEdgesButton->isChecked());
		processImage();
		});


	connect(menu->flipHorizontal, &QCheckBox::clicked, this, [&] {
		history.add(FLIP_HORIZONTAL, menu->flipHorizontal->isChecked());
		statusBar->showMessage("Flipped horizontally");
		processImage();
		});
	connect(menu->flipVertical, &QCheckBox::clicked, this, [&] {
		history.add(FLIP_VERTICAL, menu->flipVertical->isChecked());
		statusBar->showMessage("Flipped vertically");
		processImage();
		});
	connect(menu->undoBtn, &QPushButton::clicked, this, [&] {
		history.undo();
		statusBar->showMessage(QString("Undone %1").arg(history.lastChange().c_str()));
		processImage();
		setOptions();
		});
	connect(menu->redoBtn, &QPushButton::clicked, this, [&] {
		history.redo();
		statusBar->showMessage(QString("Redone %1").arg(history.lastChange().c_str()));
		processImage();
		setOptions();
		});

	connect(menu->zoomIn, &QPushButton::clicked, [&] {
		imageContainer->zoomIn();
		setOptions();
		});
	connect(menu->zoomOut, &QPushButton::clicked, [&] {
		imageContainer->zoomOut();
		if (imageContainer->getZoomCount() == 0)
			imageContainer->fitInView(&pixmap, Qt::KeepAspectRatio);
		setOptions();
		});
	connect(menu->zoomReset, &QPushButton::clicked, [&] {
		imageContainer->zoomReset();
		imageContainer->fitInView(&pixmap, Qt::KeepAspectRatio);
		setOptions();
		});

	imageContainer->setMinimumSize(640, 480);

	QVBoxLayout* vbox = new QVBoxLayout;
	QHBoxLayout* hbox = new QHBoxLayout;
	hbox->addWidget(imageContainer);
	hbox->addWidget(menu, 0);
	vbox->addLayout(hbox);
	vbox->addWidget(statusBar);
	hbox->setContentsMargins(20, 0, 20, 0);
	setLayout(vbox);
	layout()->setContentsMargins(0, 20, 0, 0);

	// set the initial values of the menu controls
	menu->flipHorizontal->setChecked(true); // the image is flipped
	menu->detectorsList->setCurrentIndex(0); // 0 = no detection, 1... = other options

	menu->binaryThresholdingButton->setChecked(false);
	menu->histogramEqualizationButton->setChecked(false);
	menu->detectEdgesButton->setChecked(false);

	// init the PixMap
	imageContainer->setScene(new QGraphicsScene);
	imageContainer->scene()->addItem(&pixmap);
	imageContainer->setAlignment(Qt::AlignCenter);

	// load labels for detectors (after being seleced they might be deleted if the corresponding detector didn't load succesfully)
	for (QString name : names) {
		menu->detectorsList->addItem(name);
	}
}

/**
 * @brief Sets the options for the menu based on the current state of the application.
 * @details This function enables or disables various menu options and sets their text or checked state based on the current state of the application. For example, if the camera is turned on, the "Turn Camera On" button will be changed to "Turn Camera Off". Similarly, if an image has been uploaded, certain options such as zooming in and out will be enabled.
 */
void MainWindow::setOptions()
{
	menu->detectorsList->setEnabled(cameraIsOn || imageIsUpload);
	menu->toggleCamera->setText("   Turn Camera " + QString(cameraIsOn ? "Off" : "On"));
	menu->toggleFaceFeatures->setVisible((cameraIsOn || imageIsUpload) && currDet != nullptr && currDet->getType() == cascade && (currDet->canDetectEyes() || currDet->canDetectSmiles()));
	menu->showConfidence->setVisible((cameraIsOn || imageIsUpload) && currDet != nullptr && currDet->getType() == network);
	menu->confControl->setVisible((cameraIsOn || imageIsUpload) && currDet != nullptr && currDet->getType() == network);
	menu->flipHorizontal->setEnabled(cameraIsOn || imageIsUpload);
	menu->flipVertical->setEnabled(cameraIsOn || imageIsUpload);
	menu->screenshot->setVisible(cameraIsOn || imageIsUpload);
	menu->thresholdControl->setVisible((cameraIsOn || imageIsUpload) && thresholdActive());
	menu->zoomIn->setEnabled(imageIsUpload);
	menu->zoomOut->setEnabled(imageIsUpload && (imageContainer->getZoomCount() > 0));
	menu->zoomReset->setEnabled(menu->zoomOut->isEnabled());

	menu->undoBtn->setEnabled(history.canUndo());
	menu->redoBtn->setEnabled(history.canRedo());

	menu->showConfidence->setChecked(history.get()->getShowConfidence());
	menu->toggleFaceFeatures->setChecked(history.get()->getShowFeatures());
	menu->flipHorizontal->setChecked(history.get()->getFlipH());
	menu->flipVertical->setChecked(history.get()->getFlipV());
	menu->binaryThresholdingButton->setChecked(history.get()->getBinaryThresholdingValue());
	menu->zeroThresholdingButton->setChecked(history.get()->getZeroThresholdingValue());
	menu->adaptiveThresholdingButton->setChecked(history.get()->getAdaptiveThresholdingValue());
	menu->histogramEqualizationButton->setChecked(history.get()->getHistogramEqualization());
	menu->detectEdgesButton->setChecked(history.get()->getDetectEdges());

	menu->binaryThresholdingButton->setEnabled(
		!menu->zeroThresholdingButton->isChecked() &&
		!menu->adaptiveThresholdingButton->isChecked()
	);
	menu->zeroThresholdingButton->setEnabled(
		!menu->binaryThresholdingButton->isChecked() &&
		!menu->adaptiveThresholdingButton->isChecked()
	);
	menu->adaptiveThresholdingButton->setEnabled(
		!menu->binaryThresholdingButton->isChecked() &&
		!menu->zeroThresholdingButton->isChecked()
	);

	menu->imageAlgorithms->setVisible(cameraIsOn || imageIsUpload);
}

/**
 * @brief Toggles the camera on or off.
 * @details This function is called when the "Turn Camera On/Off" button is clicked. It checks the state of the button and sets the cameraIsOn and imageIsUpload variables accordingly. It then calls the setOptions() function to update the menu options based on the new state. If the camera is turned on, it starts video capture and selects a detector. If the camera is turned off, it displays an image indicating that the camera is turned off.
 */
void MainWindow::toggleCameraEvent() {
	cameraIsOn = menu->toggleCamera->isChecked();
	menu->uploadButton->setChecked(false);
	imageIsUpload = menu->uploadButton->isChecked();
	setOptions();
	menu->toggleCamera->clearFocus();
	history.reset();
	imageContainer->zoomReset();

	if (cameraIsOn) {
		menu->flipHorizontal->setChecked(true);
		history.get()->setFlipH(menu->flipHorizontal->isChecked());
		menu->flipVertical->setChecked(false);
		history.get()->setFlipV(menu->flipVertical->isChecked());

		startVideoCapture();
		selectDetectorEvent();
	}
	else {
		QImage img(imageContainer->size().width(), imageContainer->size().height(), QImage::Format::Format_RGB32);
		QImage logo(":/assets/camera_dark.png");
		logo = logo.scaled(100, 100, Qt::KeepAspectRatio);

		QPainter p;
		img.fill(Qt::white);
		p.begin(&img);
		p.drawImage(imageContainer->size().width() / 2.0 - logo.size().width() / 2.0, imageContainer->size().height() / 2.0 - logo.size().height() / 2.0, logo);
		p.drawText(0, imageContainer->size().height() / 2.0 + logo.size().height() / 2.0 + 20, imageContainer->size().width(), 10, Qt::AlignCenter, "Camera is turned off");
		p.end();
		cv::Mat  mat(img.height(), img.width(), CV_8UC4, const_cast<uchar*>(img.bits()), static_cast<size_t>(img.bytesPerLine()));

		cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
		frame = mat;

		displayImage();
		delete currDet;
		currDet = nullptr;
	}
}

/**
 * @brief Returns the file name of an image selected by the user.
 * @details This function opens a file dialog and allows the user to select an image file. It returns the file name of the selected image.
 * @return A QString representing the file name of the selected image.
 */

QString MainWindow::getImageFileName()
{
	return QFileDialog::getOpenFileName(this, tr("Open Image"), QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).first(), tr("Image Files (*.png *.jpg *.bmp)"));
}

/**
 * @brief Uploads an image selected by the user.
 * @details This function is called when the "Upload Image" button is clicked. It calls the getImageFileName() function to get the file name of the selected image. If a valid image file is selected, it reads the image data and sets the frame variable to the image data. It then updates the menu options and displays the uploaded image.
 */
void MainWindow::uploadImageEvent() {
	fileName = getImageFileName();

	if (fileName.isEmpty()) {
		return;
	}
	cv::Mat image = cv::imread(fileName.toStdString());
	if (image.data != NULL)
		frame = image;
	else {
		QMessageBox::critical(this, "Error", QString("Couldnt't read image from %1. The file may be corrupted or not a valid image file.").arg(fileName));
		return;
	}
	statusBar->showMessage(QString("Uploaded file: %1").arg(fileName));

	menu->toggleCamera->setChecked(false);
	menu->flipHorizontal->setChecked(false);
	menu->flipVertical->setChecked(false);

	history.reset();
	history.get()->setFlipH(menu->flipHorizontal->isChecked());
	history.get()->setFlipV(menu->flipVertical->isChecked());

	imageIsUpload = true;
	imageContainer->zoomReset();
	setOptions();
	processImage();
	imageContainer->fitInView(&pixmap, Qt::KeepAspectRatio);
	displayImage();
}

/**
 * @brief Selects a detector from the list of available detectors.
 * @details This function is called when a detector is selected from the list of available detectors. It deletes the current detector object and sets it to nullptr. If a valid detector is selected, it attempts to load the detector from the modelsJSON file. If the detector is successfully loaded, it sets the currDet variable to the loaded detector object. If an image has been uploaded, it processes the image using the selected detector. It then calls the setOptions() function to update the menu options based on the new state.
 */
void MainWindow::selectDetectorEvent() {
	delete currDet;
	currDet = nullptr;
	if (menu->detectorsList->currentIndex() == 0)
		return;

	QString currText = menu->detectorsList->currentText();
	int index = menu->detectorsList->findText(currText);
	int loadState = ModelLoader::getFromFileByName(currDet, currText, modelsJSON);
	bool loaded = false;
	switch (loadState) {
	case ModelErrors::NAME_NOT_FOUND:
		QMessageBox::critical(this, "Model not found",
			QString("No entry named \"%1\" was found in %2")
			.arg(currText)
			.arg(modelsJSON)
		);
		break;
	case ModelErrors::TYPE_NOT_PROVIDED:
		QMessageBox::critical(this, "Type not found",
			QString("Model \"%1\" was not provided a type (cascade or neural network) in %2")
			.arg(currText)
			.arg(modelsJSON)
		);
		break;
	case ModelErrors::MODEL_PATH_EMPTY:
		QMessageBox::critical(this, "Empty path",
			QString("Model \"%1\" was not provided a path to the detection model in %2")
			.arg(currText)
			.arg(modelsJSON)
		);
		break;
	case ModelErrors::INVALID_CASCADE:
		QMessageBox::critical(this, "Couldn't load cascade file",
			QString("\"%1\" is not a valid cascade file.")
			.arg(ModelLoader::getObjectByName(currText, modelsJSON).value("paths").toObject()["face"].toString())
		);
		break;
	case ModelErrors::INF_GRAPH_PATH_EMPTY:
		QMessageBox::critical(this, "Inference graph path empty",
			QString("Model \"%1\" was not provided a path to a frozen inference graph in %2")
			.arg(currText)
			.arg(modelsJSON)
		);
		break;
	case ModelErrors::CANNOT_READ_NETWORK:
		QMessageBox::critical(this, "Couldn't read model",
			QString("Couldn't read model \"%1\". Please check to following paths in %2 lead to a valid inference graph and weights file: \n%3 \n%4")
			.arg(currText)
			.arg(modelsJSON)
			.arg(ModelLoader::getObjectByName(currText, modelsJSON).value("paths").toObject()["inf"].toString())
			.arg(ModelLoader::getObjectByName(currText, modelsJSON).value("paths").toObject()["model"].toString())
		);
		break;
	case 1:
		loaded = true;
	}

	if (!loaded) {
		menu->detectorsList->setItemIcon(index, QIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical)));
		menu->detectorsList->setCurrentIndex(0);
		delete currDet;
		currDet = nullptr;
	}
	else if (currDet->getType() == cascade && (!currDet->canDetectEyes() || !currDet->canDetectSmiles())) {
		if (menu->detectorsList->itemIcon(index).isNull()) {
			QMessageBox::information(this, "Model not completely loaded",
				QString("Model \"%1\" has only loaded cascade(s) to detect %2. If you want to be able to detect %3 you can add the paths to the cascades in %4 and reload.")
				.arg(currText)
				.arg(QString("faces%1")
					.arg(currDet->canDetectEyes() ? " and eyes" : currDet->canDetectSmiles() ? " and smiles" : ""))
				.arg(QString(!currDet->canDetectSmiles() ? currDet->canDetectEyes() ? "smiles" : "eyes or smiles" : "eyes"))
				.arg(modelsJSON)
			);
			menu->detectorsList->setItemIcon(index, QIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation)));
		}
	}
	else
		menu->detectorsList->setItemIcon(index, QIcon());
	if (imageIsUpload)
		processImage();
	setOptions();
}

/**
 * @brief Changes the minimum confidence value for object detection.
 * @details This function is called when the minimum confidence value is changed using the slider in the menu. It sets the minimum confidence value of the current detector to the value of the slider. If an image has been uploaded, it processes the image using the new minimum confidence value.
 */
void MainWindow::changeMinConfEvent() {
	currDet->setMinConfidence(menu->confControl->value() / static_cast<float>(100));
	if (imageIsUpload)
		processImage();
}

/**
 * @brief Saves a screenshot of the current image.
 * @details This function is called when the "Save Screenshot" button is clicked. It opens a file dialog and allows the user to select a file name and location to save the screenshot. If a valid file name is selected, it saves a screenshot of the current image to the specified file.
 */
void MainWindow::screenshotEvent() {
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Image File"),
		QString(),
		tr("Images (*.png)"));
	if (!fileName.isEmpty()) {
		// Re-shrink the scene to it's bounding contents
		imageContainer->scene()->setSceneRect(imageContainer->scene()->itemsBoundingRect());
		// Create the image with the exact size of the shrunk scene
		QImage image(imageContainer->scene()->sceneRect().size().toSize(), QImage::Format_ARGB32);
		// Start all pixels transparent to avoid white background
		image.fill(Qt::transparent);

		QPainter painter(&image); // paint the image over the transparent pixels
		imageContainer->scene()->render(&painter);
		image.save(fileName);
		statusBar->showMessage(QString("Saved file: %1").arg(fileName));
	}
}

/**
 * @brief Sets the detector for object detection.
 * @details This function is called when a detector is selected from the list of available detectors. If a valid detector is selected, it attempts to detect objects in the current frame using the selected detector. If objects are detected, it updates the status bar with information about the detected objects.
 */
void MainWindow::setDetector() {
	if (currDet == nullptr) return;
	try {
		if (currDet->getType() == cascade) {
			currDet->detect(frame, menu->toggleFaceFeatures->isChecked());
		}
		else if (currDet->getType() == network) {
			if (frame.type() == CV_8UC1) {
				QMessageBox::critical(this, "Error", "This detector does not work on 1-channel images");
				menu->detectorsList->setCurrentIndex(0);
				return;
			}
			currDet->detect(frame, menu->showConfidence->isChecked());
		}

		if (currDet->getLastRect().empty() == false) {
			cv::Rect rect = currDet->getLastRect();
			statusBar->showMessage(QString("Detected %5 at: <%1 %2> - <%3 %4>")
				.arg(QString::number(rect.x))
				.arg(QString::number(rect.y))
				.arg(QString::number(rect.x + rect.width))
				.arg(QString::number(rect.y + rect.height))
				.arg(QString::fromStdString(currDet->currentClassName)));
		}
		else statusBar->clearMessage();
	}
	catch (const std::exception& e) {
		QMessageBox::critical(this, "Error", e.what());
		menu->detectorsList->setCurrentIndex(0);
	}
}

/**
 * @brief Flips the current image horizontally or vertically.
 * @details This function is called when the "Flip Horizontally" or "Flip Vertically" buttons are clicked. It checks the state of the buttons and flips the current frame horizontally or vertically accordingly.
 */
void MainWindow::flipImage() {
	if (menu->flipHorizontal->isChecked()) {
		cv::flip(frame, frame, 1);
	}
	if (menu->flipVertical->isChecked()) {
		cv::flip(frame, frame, 0);
	}
}

/**
 * @brief Displays the current image in the image container.
 * @details This function converts the current frame to a QImage and sets the pixmap of the image item in the scene to the QImage. It then updates the scene rect to fit the size of the image and updates the resolution label with the resolution of the current frame.
 */
void MainWindow::displayImage() {
	QImage qimg;
	if (isGrayscale)
		qimg = QImage(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_Grayscale8);
	else
		qimg = QImage(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);

	pixmap.setPixmap(QPixmap::fromImage(qimg));
	imageContainer->scene()->setSceneRect(imageContainer->scene()->itemsBoundingRect());

	preventReset();

	QString res = QString("Resolution: %1 x %2  ")
		.arg(QString::number(frame.size().width))
		.arg(QString::number(frame.size().height));
	if (!cameraIsOn && !imageIsUpload) {
		resLabel->setText("");
		fpsLabel->setText("");
	}
	else
		resLabel->setText(res);

	QCoreApplication::processEvents();

}

/**
 * @brief Starts video capture from the camera.
 * @details This function is called when the camera is turned on. It opens a video capture object and reads frames from the camera. It processes each frame using the selected detector and displays it in the image container. It also updates the FPS label with the current FPS value.
 */
void MainWindow::startVideoCapture() {
	int fps = 0, avgFps = 0;
	std::deque<int> fpsArray;
	cv::VideoCapture cap(0);

	isGrayscale = false;

	if (!cap.isOpened()) {
		qDebug() << "Could not open video camera.";
		return;
	}

	while (cameraIsOn && imageContainer->isVisible()) {
		// measure live fps, create a queue of 60 measurements and find the average value
		Timer timer(fps);
		fpsArray.emplace_back(fps);
		if (fpsArray.size() > 60)
			fpsArray.pop_front();
		for (auto&& f : fpsArray)
			avgFps += f;
		avgFps /= fpsArray.size();

		if (!cap.read(frame))
			break;

		processImage();
		fpsLabel->setText(QString("FPS: %1   (avg: %2)  ").arg(QString::number(fps)).arg(QString::number(avgFps)));
		displayImage();
	}
	cap.release();
	QCoreApplication::processEvents();
}

/**
 * @brief Processes the current image using the selected algorithms and detector.
 * @details This function is called when an image is uploaded or when an algorithm or detector is selected. If an image has been uploaded, it reads the image data from the file. It then calls the selectAlgorithmsEvent() function to apply any selected image processing algorithms to the current frame. It flips the current frame horizontally or vertically if the corresponding buttons are clicked. It then calls the setDetector() function to detect objects in the current frame using the selected detector. If an image has been uploaded, it displays the processed image in the image container.
 */
void MainWindow::processImage() {
	isGrayscale = false;

	if (imageIsUpload)
		frame = cv::imread(fileName.toStdString());

	selectAlgorithmsEvent();
	flipImage();
	setDetector();

	if (frame.empty())
		return;

	if (imageIsUpload) displayImage();
}

/**
 * @brief Changes the threshold value for binary thresholding.
 * @details This function is called when the threshold value is changed using the slider in the menu. If an image has been uploaded, it processes the image using the new threshold value and updates the status bar with information about the applied threshold value.
 */
void MainWindow::changeThresholdEvent() {
	if (imageIsUpload)
		processImage();
	statusBar->showMessage(QString("Applied binary thresholding: %1").arg(menu->thresholdControl->value()));
}

/**
 * @brief Prevents the zoom level from being reset when other actions are applied to the image.
 * @details This function is called when other actions are applied to the image or when the window is resized. It fits the image in view and maintains the current zoom level.
 */
void MainWindow::preventReset() {
	// prevent the container from losing the zoom applying other actions on the image (e.g. flip) or on resize
	imageContainer->fitInView(&pixmap, Qt::KeepAspectRatio);
	if (imageContainer->getZoomCount() > 0) {
		int temp = imageContainer->getZoomCount();
		imageContainer->zoomReset();
		imageContainer->zoomIn(temp);
	}
}

/**
 * @brief Applies selected image processing algorithms to the current frame.
 * @details This function is called when an image processing algorithm is selected from the list of available algorithms. It checks the state of the algorithm buttons and applies the selected algorithms to the current frame in the order in which they are listed in the menu. For example, if the "Histogram Equalization" and "Binary Thresholding" buttons are both checked, it will first apply histogram equalization to the current frame and then apply binary thresholding to the result.
 */
void MainWindow::selectAlgorithmsEvent()
{
	setOptions();
	for (QPushButton* btn : menu->imageAlgorithms->findChildren<QPushButton*>())
		if (btn->isChecked()) {
			break;
		}

	if (!isGrayscale && (menu->binaryThresholdingButton->isChecked() || menu->histogramEqualizationButton->isChecked() || menu->adaptiveThresholdingButton->isChecked())) {
		isGrayscale = true;
		cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
	}

	if (menu->histogramEqualizationButton->isChecked())
		histogramEqualization(frame);
	if (menu->binaryThresholdingButton->isChecked())
		binaryThresholding(frame, menu->thresholdControl->value());
	if (menu->adaptiveThresholdingButton->isChecked())
		adaptiveThresholding(frame, menu->thresholdControl->value());

	if (menu->zeroThresholdingButton->isChecked()) {
		if (isGrayscale) {
			cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
			isGrayscale = false;

		}
		zeroThresholding(frame, menu->thresholdControl->value());
	}
	if (menu->detectEdgesButton->isChecked()) {
		if (isGrayscale) {
			cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);

			isGrayscale = false;
		}
		detectEdges(frame);
	}
}
