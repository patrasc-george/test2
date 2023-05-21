#pragma once

#include <QWidget>
#include <QComboBox>
#include <QGroupBox>

#include "components.h"

class Menu : public QWidget {
public:
	QPushButton* toggleCamera;
	QCheckBox* toggleFaceFeatures;
	QCheckBox* showConfidence;
	QComboBox* detectorsList;
	QPushButton* screenshot;
	LabeledSlider* confControl;
	LabeledSlider* thresholdControl;
	QPushButton* uploadButton;

	QPushButton* zoomIn;
	QPushButton* zoomOut;
	QPushButton* zoomReset;

	QPushButton* flipHorizontal;
	QPushButton* flipVertical;

	QPushButton* undoBtn;
	QPushButton* redoBtn;

	QGroupBox* imageAlgorithms;
	QPushButton* binaryThresholdingButton;
	QPushButton* zeroThresholdingButton;
	QPushButton* adaptiveThresholdingButton;
	QPushButton* histogramEqualizationButton;
	QPushButton* detectEdgesButton;


public:
	/**
	 * @brief Constructs a Menu object.
	 * @details This constructor initializes a Menu object with the provided parent widget.
	 It creates and initializes various controls such as buttons, checkboxes, combo boxes, sliders, and group boxes.
	 These controls are used to interact with the application and control its behavior.
	 The controls are arranged in a vertical box layout and added to the Menu widget.
	 * @param[in] parent The parent widget of the Menu object.
	 */
	Menu(QWidget* parent = nullptr);
};