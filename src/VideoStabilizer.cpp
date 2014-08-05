// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#define _USE_MATH_DEFINES
#include <cmath>

#include <QElapsedTimer>

#include "VideoStabilizer.h"
#include "Settings.h"
#include "FrameData.h"

#define sign(a) (((a) < 0) ? -1 : ((a) > 0))

using namespace OrientView;

bool VideoStabilizer::initialize(Settings* settings)
{
	qDebug("Initializing VideoStabilizer");

	isFirstImage = true;
	isEnabled = settings->stabilizer.enabled;

	currentX = 0.0;
	currentY = 0.0;
	currentAngle = 0.0;
	normalizedX = 0.0;
	normalizedY = 0.0;
	normalizedAngle = 0.0;

	dampingFactor = settings->stabilizer.dampingFactor;

	currentXAverage.reset();
	currentXAverage.setAlpha(settings->stabilizer.averagingFactor);
	currentYAverage.reset();
	currentYAverage.setAlpha(settings->stabilizer.averagingFactor);
	currentAngleAverage.reset();
	currentAngleAverage.setAlpha(settings->stabilizer.averagingFactor);

	previousTransformation = cv::Mat::eye(2, 3, CV_64F);

	lastProcessTime = 0.0;

	if (outputData)
	{
		dataOutputFile.setFileName("stabilizer.txt");
		dataOutputFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
		dataOutputFile.write("currentX;currentXAverage;normalizedX;currentY;currentYAverage;normalizedY;currentAngle;currentAngleAverage;normalizedAngle\n");
	}

	return true;
}

void VideoStabilizer::shutdown()
{
	qDebug("Shutting down VideoStabilizer");

	if (dataOutputFile.isOpen())
		dataOutputFile.close();
}

void VideoStabilizer::processFrame(FrameData* frameDataGrayscale)
{
	if (!isEnabled)
		return;

	processTimer.restart();

	cv::Mat currentImage(frameDataGrayscale->height, frameDataGrayscale->width, CV_8UC1, frameDataGrayscale->data);

	if (isFirstImage)
	{
		previousImage = cv::Mat(frameDataGrayscale->height, frameDataGrayscale->width, CV_8UC1);
		currentImage.copyTo(previousImage);
		isFirstImage = false;

		return;
	}

	cv::goodFeaturesToTrack(previousImage, previousCorners, 200, 0.01, 30.0);
	cv::calcOpticalFlowPyrLK(previousImage, currentImage, previousCorners, currentCorners, opticalFlowStatus, opticalFlowError);
	
	currentImage.copyTo(previousImage);

	for (int i = 0; i < opticalFlowStatus.size(); i++)
	{
		if (opticalFlowStatus[i])
		{
			previousCornersFiltered.push_back(previousCorners[i]);
			currentCornersFiltered.push_back(currentCorners[i]);
		}
	}

	cv::Mat currentTransformation;
	currentTransformation = cv::estimateRigidTransform(previousCornersFiltered, currentCornersFiltered, false);

	if (currentTransformation.data == nullptr)
		previousTransformation.copyTo(currentTransformation);

	// a b tx
	// c d ty
	double a = currentTransformation.at<double>(0, 0);
	double b = currentTransformation.at<double>(0, 1);
	double c = currentTransformation.at<double>(1, 0);
	double d = currentTransformation.at<double>(1, 1);
	double tx = currentTransformation.at<double>(0, 2);
	double ty = currentTransformation.at<double>(1, 2);

	currentTransformation.copyTo(previousTransformation);

	double dx = tx / frameDataGrayscale->width;
	double dy = ty / frameDataGrayscale->height;
	double da = atan2(c, d) * 180.0 / M_PI;
	double ds = sign(a) * sqrt(a * a + b * b);

	currentX += dx;
	currentY += dy;
	currentAngle += da;

	normalizedX = (currentXAverage.getAverage() - currentX) * dampingFactor;
	normalizedY = (currentYAverage.getAverage() - currentY) * dampingFactor;
	normalizedAngle = (currentAngleAverage.getAverage() - currentAngle) * dampingFactor;

	currentXAverage.addMeasurement(currentX);
	currentYAverage.addMeasurement(currentY);
	currentAngleAverage.addMeasurement(currentAngle);

	if (outputData)
	{
		char buffer[1024];
		sprintf(buffer, "%f;%f;%f;%f;%f;%f;%f;%f;%f;\n", currentX, currentXAverage.getAverage(), normalizedX, currentY, currentYAverage.getAverage(), normalizedY, currentAngle, currentAngleAverage.getAverage(), normalizedAngle);
		dataOutputFile.write(buffer);
	}

	previousCorners.clear();
	currentCorners.clear();
	previousCornersFiltered.clear();
	currentCornersFiltered.clear();
	opticalFlowStatus.clear();
	opticalFlowError.clear();

	lastProcessTime = processTimer.nsecsElapsed() / 1000000.0;
}

double VideoStabilizer::getX() const
{
	return normalizedX;
}

double VideoStabilizer::getY() const
{
	return normalizedY;
}

double VideoStabilizer::getAngle() const
{
	return normalizedAngle;
}

double VideoStabilizer::getLastProcessTime() const
{
	return lastProcessTime;
}
