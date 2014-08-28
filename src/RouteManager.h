// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <vector>

#include <QColor>
#include <QPainterPath>

#include "RoutePoint.h"
#include "SplitsManager.h"

namespace OrientView
{
	class QuickRouteReader;
	class Renderer;
	class Settings;

	enum RouteRenderMode { Discreet, Highlight, Pace, None };

	struct SplitTransformation
	{
		double x = 0.0;
		double y = 0.0;
		double angle = 0.0;
		double angleDelta = 0.0;
		double scale = 1.0;
	};

	struct RouteVertex
	{
		float x = 0.0f;
		float y = 0.0f;
		float u = 0.0f;
		float v = 0.0f;
		float paceR = 0.0f;
		float paceG = 0.0f;
		float paceB = 0.0f;
		float paceA = 1.0f;
	};

	struct Route
	{
		std::vector<RoutePoint> routePoints;
		std::vector<RoutePoint> alignedRoutePoints;
		std::vector<SplitTransformation> splitTransformations;
		RunnerInfo runnerInfo;

		QPainterPath wholeRoutePath;
		RouteRenderMode wholeRouteRenderMode = RouteRenderMode::Discreet;
		QColor wholeRouteDiscreetColor = QColor(80, 0, 0, 50);
		QColor wholeRouteHighlightColor = QColor(0, 0, 255, 50);
		double wholeRouteWidth = 10.0;

		QPainterPath tailPath;
		RouteRenderMode tailRenderMode = RouteRenderMode::None;
		QColor tailDiscreetColor = QColor(80, 0, 0, 50);
		QColor tailHighlightColor = QColor(0, 0, 255, 50);
		double tailWidth = 10.0;
		double tailLength = 60.0;

		std::vector<QPointF> controlPositions;
		QColor controlBorderColor = QColor(140, 40, 140, 255);
		double controlRadius = 15.0;
		double controlBorderWidth = 5.0;
		bool showControls = true;

		QPointF runnerPosition;
		QColor runnerColor = QColor(0, 100, 255, 220);
		QColor runnerBorderColor = QColor(0, 0, 0, 255);
		double runnerBorderWidth = 1.0;
		double runnerScale = 1.0;
		bool showRunner = true;

		double controlTimeOffset = 0.0;
		double runnerTimeOffset = 0.0;
		double userScale = 1.0;
		double topBottomMargin = 30.0;
		double leftRightMargin = 10.0;
		double minimumZoom = 0.0;
		double maximumZoom = 9999.0;
		double lowPace = 15.0;
		double highPace = 5.0;

		bool useSmoothTransition = true;
		double smoothTransitionSpeed = 0.001;
		SplitTransformation currentSplitTransformation;
		SplitTransformation previousSplitTransformation;
		SplitTransformation nextSplitTransformation;
		int currentSplitTransformationIndex = -1;
		double transitionAlpha = 0.0;
		bool transitionInProgress = false;
	};

	class RouteManager
	{

	public:

		bool initialize(QuickRouteReader* quickRouteReader, SplitsManager* splitsManager, Renderer* renderer, Settings* settings);
		void update(double currentTime, double frameTime);

		void requestFullUpdate();
		void requestInstantTransition();
		void windowResized(double newWidth, double newHeight);
		double getX() const;
		double getY() const;
		double getScale() const;
		double getAngle() const;
		Route& getDefaultRoute();

	private:

		void calculateAlignedRoutePoints(Route& route);
		void calculateRoutePointColors(Route& route);
		void calculateWholeRoutePath(Route& route);
		void calculateTailPath(Route& route, double currentTime);
		void calculateControlPositions(Route& route);
		void calculateSplitTransformations(Route& route);
		void calculateCurrentRunnerPosition(Route& route, double currentTime);
		void calculateCurrentSplitTransformation(Route& route, double currentTime, double frameTime);

		RoutePoint getInterpolatedRoutePoint(Route& route, double time);
		QColor interpolateFromGreenToRed(double greenValue, double redValue, double value);

		Renderer* renderer = nullptr;

		std::vector<Route> routes;

		bool fullUpdateRequested = true;
		bool instantTransitionRequested = true;

		double windowWidth = 0.0;
		double windowHeight = 0.0;
	};
}
