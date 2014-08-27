// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#define _USE_MATH_DEFINES
#include <cmath>

#include "RouteManager.h"
#include "QuickRouteReader.h"
#include "Renderer.h"
#include "Settings.h"

using namespace OrientView;

bool RouteManager::initialize(QuickRouteReader* quickRouteReader, SplitsManager* splitsManager, Renderer* renderer, Settings* settings)
{
	this->renderer = renderer;

	windowWidth = settings->window.width;
	windowHeight = settings->window.height;

	routes.push_back(Route());
	Route& defaultRoute = routes.at(0);

	defaultRoute.routePoints = quickRouteReader->getRoutePoints();
	defaultRoute.runnerInfo = splitsManager->getDefaultRunnerInfo();
	defaultRoute.renderMode = settings->route.renderMode;
	defaultRoute.color = settings->route.color;
	defaultRoute.width = settings->route.width;
	defaultRoute.borderWidth = settings->route.borderWidth;
	defaultRoute.controlBorderColor = settings->route.controlBorderColor;
	defaultRoute.controlRadius = settings->route.controlRadius;
	defaultRoute.controlBorderWidth = settings->route.controlBorderWidth;
	defaultRoute.showControls = settings->route.showControls;
	defaultRoute.runnerColor = settings->route.runnerColor;
	defaultRoute.runnerBorderColor = settings->route.runnerBorderColor;
	defaultRoute.runnerBorderWidth = settings->route.runnerBorderWidth;
	defaultRoute.runnerScale = settings->route.runnerScale;
	defaultRoute.showRunner = settings->route.showRunner;
	defaultRoute.controlTimeOffset = settings->route.controlTimeOffset;
	defaultRoute.runnerTimeOffset = settings->route.runnerTimeOffset;
	defaultRoute.userScale = settings->route.scale;
	defaultRoute.minimumZoom = settings->route.minimumZoom;
	defaultRoute.maximumZoom = settings->route.maximumZoom;
	defaultRoute.topBottomMargin = settings->route.topBottomMargin;
	defaultRoute.leftRightMargin = settings->route.leftRightMargin;
	defaultRoute.lowPace = settings->route.lowPace;
	defaultRoute.highPace = settings->route.highPace;
	defaultRoute.useSmoothTransition = settings->route.useSmoothTransition;
	defaultRoute.smoothTransitionSpeed = settings->route.smoothTransitionSpeed;

	for (Route& route : routes)
	{
		generateAlignedRoutePoints(route);
		calculateRoutePointColors(route);

		if (!initializeShaderAndBuffer(route))
			return false;
	}

	update(0.0, 0.0);

	if (defaultRoute.currentSplitTransformationIndex == -1 && defaultRoute.splitTransformations.size() > 0)
	{
		defaultRoute.currentSplitTransformation = defaultRoute.splitTransformations.at(0);
		defaultRoute.currentSplitTransformationIndex = 0;
	}

	return true;
}

RouteManager::~RouteManager()
{
	for (Route& route : routes)
	{
		if (route.shaderProgram != nullptr)
		{
			delete route.shaderProgram;
			route.shaderProgram = nullptr;
		}

		if (route.vertexArrayObject != nullptr)
		{
			delete route.vertexArrayObject;
			route.vertexArrayObject = nullptr;
		}

		if (route.vertexBuffer != nullptr)
		{
			delete route.vertexBuffer;
			route.vertexBuffer = nullptr;
		}
	}
}

void RouteManager::update(double currentTime, double frameTime)
{
	if (fullUpdateRequested)
	{
		for (Route& route : routes)
		{
			calculateControlPositions(route);
			calculateSplitTransformations(route);
		}

		fullUpdateRequested = false;
	}

	for (Route& route : routes)
	{
		calculateCurrentRunnerPosition(route, currentTime);
		calculateCurrentSplitTransformation(route, currentTime, frameTime);
	}
}

void RouteManager::generateAlignedRoutePoints(Route& route)
{
	if (route.routePoints.size() < 2)
		return;

	double alignedTime = 0.0;
	RoutePoint currentRoutePoint = route.routePoints.at(0);
	RoutePoint alignedRoutePoint;

	// align and interpolate route point data to one second intervals
	for (int i = 0; i < (int)route.routePoints.size() - 1;)
	{
		int nextIndex = 0;

		for (int j = i + 1; j < (int)route.routePoints.size(); ++j)
		{
			if (route.routePoints.at(j).time - currentRoutePoint.time > 1.0)
			{
				nextIndex = j;
				break;
			}
		}

		if (nextIndex <= i)
			break;

		i = nextIndex;

		RoutePoint nextRoutePoint = route.routePoints.at(nextIndex);

		alignedRoutePoint.dateTime = currentRoutePoint.dateTime;
		alignedRoutePoint.coordinate = currentRoutePoint.coordinate;

		double timeDelta = nextRoutePoint.time - currentRoutePoint.time;
		double alphaStep = 1.0 / timeDelta;
		double alpha = 0.0;
		int stepCount = (int)timeDelta;

		for (int k = 0; k <= stepCount; ++k)
		{
			alignedRoutePoint.time = alignedTime;
			alignedRoutePoint.position.setX((1.0 - alpha) * currentRoutePoint.position.x() + alpha * nextRoutePoint.position.x());
			alignedRoutePoint.position.setY((1.0 - alpha) * currentRoutePoint.position.y() + alpha * nextRoutePoint.position.y());
			alignedRoutePoint.elevation = (1.0 - alpha) * currentRoutePoint.elevation + alpha * nextRoutePoint.elevation;
			alignedRoutePoint.heartRate = (1.0 - alpha) * currentRoutePoint.heartRate + alpha * nextRoutePoint.heartRate;
			alignedRoutePoint.pace = (1.0 - alpha) * currentRoutePoint.pace + alpha * nextRoutePoint.pace;

			alpha += alphaStep;

			if (k < stepCount)
			{
				route.alignedRoutePoints.push_back(alignedRoutePoint);
				alignedTime += 1.0;
			}
		}

		currentRoutePoint = alignedRoutePoint;
		currentRoutePoint.dateTime = nextRoutePoint.dateTime;
		currentRoutePoint.coordinate = nextRoutePoint.coordinate;
	}

	route.alignedRoutePoints.push_back(alignedRoutePoint);
}

void RouteManager::calculateRoutePointColors(Route& route)
{
	for (RoutePoint& rp : route.routePoints)
		rp.color = interpolateFromGreenToRed(route.highPace, route.lowPace, rp.pace);

	for (RoutePoint& rp : route.alignedRoutePoints)
		rp.color = interpolateFromGreenToRed(route.highPace, route.lowPace, rp.pace);
}

bool RouteManager::initializeShaderAndBuffer(Route& route)
{
	std::vector<RouteVertex> routeVertices = generateRouteVertices(route);
	route.vertexCount = routeVertices.size();

	route.shaderProgram = new QOpenGLShaderProgram();
	route.vertexArrayObject = new QOpenGLVertexArrayObject();
	route.vertexBuffer = new QOpenGLBuffer();

	route.vertexBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
	route.vertexBuffer->create();
	route.vertexBuffer->bind();
	route.vertexBuffer->allocate(routeVertices.data(), sizeof(RouteVertex) * routeVertices.size());
	route.vertexBuffer->release();

	if (!route.shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "data/shaders/route.vert"))
		return false;

	if (!route.shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "data/shaders/route.frag"))
		return false;

	if (!route.shaderProgram->link())
		return false;

	route.vertexArrayObject->create();
	route.vertexArrayObject->bind();

	route.vertexBuffer->bind();
	route.shaderProgram->enableAttributeArray("vertexPosition");
	route.shaderProgram->enableAttributeArray("vertexTextureCoordinate");
	route.shaderProgram->enableAttributeArray("vertexColor");
	route.shaderProgram->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 2, sizeof(GLfloat) * 8);
	route.shaderProgram->setAttributeBuffer("vertexTextureCoordinate", GL_FLOAT, sizeof(GLfloat) * 2, 2, sizeof(GLfloat) * 8);
	route.shaderProgram->setAttributeBuffer("vertexColor", GL_FLOAT, sizeof(GLfloat) * 4, 4, sizeof(GLfloat) * 8);

	route.vertexArrayObject->release();
	route.vertexBuffer->release();

	return true;
}

void RouteManager::calculateControlPositions(Route& route)
{
	route.controlPositions.clear();

	for (const Split& split : route.runnerInfo.splits)
	{
		RoutePoint rp = getInterpolatedRoutePoint(route, split.absoluteTime + route.controlTimeOffset);
		route.controlPositions.push_back(rp.position);
	}
}

void RouteManager::calculateSplitTransformations(Route& route)
{
	if (route.runnerInfo.splits.empty() || route.alignedRoutePoints.empty())
		return;

	route.splitTransformations.clear();

	// take two consecutive controls and then figure out the transformation needed
	// to make the line from start to stop control vertical, centered and zoomed appropriately
	for (int i = 0; i < (int)route.runnerInfo.splits.size() - 1; ++i)
	{
		Split split1 = route.runnerInfo.splits.at(i);
		Split split2 = route.runnerInfo.splits.at(i + 1);

		int startIndex = (int)round(split1.absoluteTime + route.controlTimeOffset);
		int stopIndex = (int)round(split2.absoluteTime + route.controlTimeOffset);
		int indexMax = (int)route.alignedRoutePoints.size() - 1;

		startIndex = std::max(0, std::min(startIndex, indexMax));
		stopIndex = std::max(0, std::min(stopIndex, indexMax));

		SplitTransformation splitTransformation;

		if (startIndex != stopIndex)
		{
			RoutePoint startRoutePoint = route.alignedRoutePoints.at(startIndex);
			RoutePoint stopRoutePoint = route.alignedRoutePoints.at(stopIndex);
			QPointF startToStop = stopRoutePoint.position - startRoutePoint.position; // vector pointing from start to stop

			// rotate towards positive y-axis
			double angle = atan2(-startToStop.y(), startToStop.x());
			angle *= (180.0 / M_PI);
			angle = 90.0 - angle;

			// offset so that left quadrants rotate cw and right quadrants ccw
			if (angle > 180.0)
				angle = angle - 360.0;

			QMatrix rotateMatrix;
			rotateMatrix.rotate(-angle);

			double minX = std::numeric_limits<double>::max();
			double maxX = -std::numeric_limits<double>::max();
			double minY = std::numeric_limits<double>::max();
			double maxY = -std::numeric_limits<double>::max();

			// find the bounding box for the split route
			for (int j = startIndex; j <= stopIndex; ++j)
			{
				// points need to be rotated
				QPointF position = rotateMatrix.map(route.alignedRoutePoints.at(j).position);

				minX = std::min(minX, position.x());
				maxX = std::max(maxX, position.x());
				minY = std::min(minY, position.y());
				maxY = std::max(maxY, position.y());
			}

			QPointF startPosition = rotateMatrix.map(startRoutePoint.position); // rotated starting position
			QPointF middlePoint = (startRoutePoint.position + stopRoutePoint.position) / 2.0; // doesn't need to be rotated

			// split width is taken from the maximum deviation from center line to either left or right side
			double splitWidthLeft = abs(minX - startPosition.x()) * 2.0 + 2.0 * route.leftRightMargin;
			double splitWidthRight = abs(maxX - startPosition.x()) * 2.0 + 2.0 * route.leftRightMargin;
			double splitWidth = std::max(splitWidthLeft, splitWidthRight);

			// split height is the maximum vertical delta
			double splitHeight = maxY - minY + 2.0 * route.topBottomMargin;

			double scaleX = (windowWidth * renderer->getMapPanel().relativeWidth) / splitWidth;
			double scaleY = windowHeight / splitHeight;
			double finalScale = std::min(scaleX, scaleY);
			finalScale = std::max(route.minimumZoom, std::min(finalScale, route.maximumZoom));

			splitTransformation.x = -middlePoint.x();
			splitTransformation.y = middlePoint.y();
			splitTransformation.angle = angle;
			splitTransformation.scale = finalScale;
		}

		route.splitTransformations.push_back(splitTransformation);
	}

	instantTransitionRequested = true;
}

void RouteManager::calculateCurrentRunnerPosition(Route& route, double currentTime)
{
	RoutePoint rp = getInterpolatedRoutePoint(route, currentTime + route.runnerTimeOffset);
	route.runnerPosition = rp.position;
}

void RouteManager::calculateCurrentSplitTransformation(Route& route, double currentTime, double frameTime)
{
	for (int i = 0; i < (int)route.runnerInfo.splits.size() - 1; ++i)
	{
		double firstSplitOffsetTime = route.runnerInfo.splits.at(i).absoluteTime + route.controlTimeOffset;
		double secondSplitOffsetTime = route.runnerInfo.splits.at(i + 1).absoluteTime + route.controlTimeOffset;
		double runnerOffsetTime = currentTime + route.runnerTimeOffset;

		// check if we are inside the time range of two consecutive controls
		if (runnerOffsetTime >= firstSplitOffsetTime && runnerOffsetTime < secondSplitOffsetTime)
		{
			if (i >= (int)route.splitTransformations.size())
				break;

			if (instantTransitionRequested)
			{
				route.currentSplitTransformation = route.splitTransformations.at(i);
				route.currentSplitTransformationIndex = i;
				instantTransitionRequested = false;
			}
			else if (i != route.currentSplitTransformationIndex)
			{
				if (route.useSmoothTransition)
				{
					route.previousSplitTransformation = route.currentSplitTransformation;
					route.nextSplitTransformation = route.splitTransformations.at(i);
					route.transitionAlpha = 0.0;
					route.transitionInProgress = true;

					double angleDelta = route.nextSplitTransformation.angle - route.previousSplitTransformation.angle;
					double absoluteAngleDelta = abs(angleDelta);
					double finalAngleDelta = angleDelta;

					// always try to rotate as little as possible
					if (absoluteAngleDelta > 180.0)
					{
						finalAngleDelta = 360.0 - absoluteAngleDelta;
						finalAngleDelta *= (angleDelta < 0.0) ? 1.0 : -1.0;
					}

					route.previousSplitTransformation.angleDelta = finalAngleDelta;
				}
				else
					route.currentSplitTransformation = route.splitTransformations.at(i);

				route.currentSplitTransformationIndex = i;
			}

			break;
		}
	}

	if (route.useSmoothTransition && route.transitionInProgress)
	{
		if (route.transitionAlpha > 1.0)
		{
			route.currentSplitTransformation = route.nextSplitTransformation;
			route.transitionInProgress = false;
		}
		else
		{
			double alpha = route.transitionAlpha;
			alpha = alpha * alpha * alpha * (alpha * (alpha * 6 - 15) + 10); // smootherstep

			route.currentSplitTransformation.x = (1.0 - alpha) * route.previousSplitTransformation.x + alpha * route.nextSplitTransformation.x;
			route.currentSplitTransformation.y = (1.0 - alpha) * route.previousSplitTransformation.y + alpha * route.nextSplitTransformation.y;
			route.currentSplitTransformation.angle = route.previousSplitTransformation.angle + alpha * route.previousSplitTransformation.angleDelta;
			route.currentSplitTransformation.scale = (1.0 - alpha) * route.previousSplitTransformation.scale + alpha * route.nextSplitTransformation.scale;

			route.transitionAlpha += route.smoothTransitionSpeed * frameTime;
		}
	}
}

std::vector<RouteVertex> RouteManager::generateRouteVertices(Route& route)
{
	std::vector<RouteVertex> routeVertices;

	if (route.alignedRoutePoints.size() < 2)
		return routeVertices;

	QPointF previousTlVertex, previousTrVertex;
	RouteVertex previousTlRouteVertex, previousTrRouteVertex;
	double previousAngle = 0.0;

	for (int i = 0; i < (int)route.alignedRoutePoints.size() - 1;)
	{
		RoutePoint rp1 = route.alignedRoutePoints.at(i);
		RoutePoint rp2;
		QPointF routePointVector;

		for (int j = i + 1; j < (int)route.alignedRoutePoints.size(); ++j)
		{
			i = j;

			rp2 = route.alignedRoutePoints.at(j);
			routePointVector = rp2.position - rp1.position;
			double length = sqrt(routePointVector.x() * routePointVector.x() + routePointVector.y() * routePointVector.y());

			if (length > route.width)
				break;
		}

		double angle = atan2(-routePointVector.y(), routePointVector.x());
		double angleDelta = angle - previousAngle;
		double absoluteAngleDelta = abs(angleDelta);
		double finalAngleDelta = angleDelta;

		if (absoluteAngleDelta > M_PI)
		{
			finalAngleDelta = 2.0 * M_PI - absoluteAngleDelta;
			finalAngleDelta *= (angleDelta < 0.0) ? 1.0 : -1.0;
		}

		QPointF deltaVertex;
		deltaVertex.setX(sin(angle) * route.width);
		deltaVertex.setY(cos(angle) * route.width);

		if (i == 0)
		{
			previousTlVertex = rp1.position + deltaVertex;
			previousTrVertex = rp1.position - deltaVertex;
		}

		QPointF blVertex;
		QPointF brVertex;
		QPointF tlVertex = rp2.position + deltaVertex;
		QPointF trVertex = rp2.position - deltaVertex;

		RouteVertex blRouteVertex, brRouteVertex, tlRouteVertex, trRouteVertex;

		if (finalAngleDelta > 0.0)
		{
			blVertex = previousTrVertex + 2.0 * deltaVertex;
			brVertex = previousTrVertex;
		}
		else
		{
			blVertex = previousTlVertex;
			brVertex = previousTlVertex - 2.0 * deltaVertex;
		}

		blRouteVertex.x = blVertex.x();
		blRouteVertex.y = -blVertex.y();
		blRouteVertex.u = -1.0f;

		brRouteVertex.x = brVertex.x();
		brRouteVertex.y = -brVertex.y();
		brRouteVertex.u = 1.0f;

		tlRouteVertex.x = tlVertex.x();
		tlRouteVertex.y = -tlVertex.y();
		tlRouteVertex.u = -1.0f;

		trRouteVertex.x = trVertex.x();
		trRouteVertex.y = -trVertex.y();
		trRouteVertex.u = 1.0f;

		blRouteVertex.r = brRouteVertex.r = rp1.color.redF();
		blRouteVertex.g = brRouteVertex.g = rp1.color.greenF();
		blRouteVertex.b = brRouteVertex.b = rp1.color.blueF();
		blRouteVertex.a = brRouteVertex.a = rp1.color.alphaF();

		tlRouteVertex.r = trRouteVertex.r = rp2.color.redF();
		tlRouteVertex.g = trRouteVertex.g = rp2.color.greenF();
		tlRouteVertex.b = trRouteVertex.b = rp2.color.blueF();
		tlRouteVertex.a = trRouteVertex.a = rp2.color.alphaF();

		RouteVertex jointOrigoRouteVertex, jointStartRouteVertex, jointEndRouteVertex;

		if (finalAngleDelta > 0.0)
		{
			jointOrigoRouteVertex = brRouteVertex;
			jointStartRouteVertex = previousTlRouteVertex;
			jointEndRouteVertex = blRouteVertex;
		}
		else
		{
			jointOrigoRouteVertex = blRouteVertex;
			jointStartRouteVertex = previousTrRouteVertex;
			jointEndRouteVertex = brRouteVertex;
		}

		//routeVertices.push_back(jointOrigoRouteVertex);
		//routeVertices.push_back(jointStartRouteVertex);
		//routeVertices.push_back(jointEndRouteVertex);
		routeVertices.push_back(blRouteVertex);
		routeVertices.push_back(brRouteVertex);
		routeVertices.push_back(trRouteVertex);
		routeVertices.push_back(blRouteVertex);
		routeVertices.push_back(trRouteVertex);
		routeVertices.push_back(tlRouteVertex);

		previousTlVertex = tlVertex;
		previousTrVertex = trVertex;
		previousTlRouteVertex = tlRouteVertex;
		previousTrRouteVertex = trRouteVertex;
		previousAngle = angle;
	}

	return routeVertices;
}

RoutePoint RouteManager::getInterpolatedRoutePoint(Route& route, double time)
{
	if (route.alignedRoutePoints.empty())
		return RoutePoint();

	double previousWholeSecond = floor(time);
	double alpha = time - previousWholeSecond;

	int firstIndex = (int)previousWholeSecond;
	int secondIndex = firstIndex + 1;
	int indexMax = (int)route.alignedRoutePoints.size() - 1;

	firstIndex = std::max(0, std::min(firstIndex, indexMax));
	secondIndex = std::max(0, std::min(secondIndex, indexMax));

	if (firstIndex == secondIndex)
		return route.alignedRoutePoints.at(firstIndex);
	else
	{
		RoutePoint firstRoutePoint = route.alignedRoutePoints.at(firstIndex);
		RoutePoint secondRoutePoint = route.alignedRoutePoints.at(secondIndex);
		RoutePoint interpolatedRoutePoint = firstRoutePoint;

		interpolatedRoutePoint.time = time;
		interpolatedRoutePoint.position = (1.0 - alpha) * firstRoutePoint.position + alpha * secondRoutePoint.position;
		interpolatedRoutePoint.elevation = (1.0 - alpha) * firstRoutePoint.elevation + alpha * secondRoutePoint.elevation;
		interpolatedRoutePoint.heartRate = (1.0 - alpha) * firstRoutePoint.heartRate + alpha * secondRoutePoint.heartRate;
		interpolatedRoutePoint.pace = (1.0 - alpha) * firstRoutePoint.pace + alpha * secondRoutePoint.pace;
		interpolatedRoutePoint.color = interpolateFromGreenToRed(route.highPace, route.lowPace, interpolatedRoutePoint.pace);

		return interpolatedRoutePoint;
	}
}

QColor RouteManager::interpolateFromGreenToRed(double greenValue, double redValue, double value)
{
	double alpha = (value - greenValue) / (redValue - greenValue);
	alpha = std::max(0.0, std::min(alpha, 1.0));

	double r = (alpha > 0.5 ? 1.0 : 2.0 * alpha);
	double g = (alpha > 0.5 ? 1.0 - 2.0 * (alpha - 0.5) : 1.0);
	double b = 0.0;

	return QColor::fromRgbF(r, g, b);
}

void RouteManager::requestFullUpdate()
{
	fullUpdateRequested = true;
}

void RouteManager::requestInstantTransition()
{
	instantTransitionRequested = true;
}

void RouteManager::windowResized(double newWidth, double newHeight)
{
	windowWidth = newWidth;
	windowHeight = newHeight;

	fullUpdateRequested = true;
}

double RouteManager::getX() const
{
	return routes.at(0).currentSplitTransformation.x;
}

double RouteManager::getY() const
{
	return routes.at(0).currentSplitTransformation.y;
}

double RouteManager::getScale() const
{
	return routes.at(0).currentSplitTransformation.scale;
}

double RouteManager::getAngle() const
{
	return routes.at(0).currentSplitTransformation.angle;
}

Route& RouteManager::getDefaultRoute()
{
	return routes.at(0);
}
