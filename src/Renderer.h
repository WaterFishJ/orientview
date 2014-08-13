// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLFramebufferObject>

#include "RoutePoint.h"
#include "MovingAverage.h"
#include "FrameData.h"

namespace OrientView
{
	class VideoDecoder;
	class QuickRouteReader;
	class MapImageReader;
	class VideoStabilizer;
	class InputHandler;
	class Settings;
	struct Panel;

	enum class RenderMode { ALL, VIDEO, MAP };

	struct Panel
	{
		QOpenGLShaderProgram* program = nullptr;
		QOpenGLBuffer* buffer = nullptr;
		QOpenGLTexture* texture = nullptr;

		QMatrix4x4 vertexMatrix;

		QColor clearColor = QColor(0, 0, 0);
		bool clippingEnabled = true;
		bool clearingEnabled = true;

		double x = 0.0;
		double y = 0.0;
		double angle = 0.0;
		double scale = 1.0;
		double userX = 0.0;
		double userY = 0.0;
		double userAngle = 0.0;
		double userScale = 1.0;

		double textureWidth = 0.0;
		double textureHeight = 0.0;
		double texelWidth = 0.0;
		double texelHeight = 0.0;

		double relativeWidth = 1.0;

		int vertexMatrixUniform = 0;
		int vertexPositionAttribute = 0;
		int vertexTextureCoordinateAttribute = 0;
		int textureSamplerUniform = 0;
		int textureWidthUniform = 0;
		int textureHeightUniform = 0;
		int texelWidthUniform = 0;
		int texelHeightUniform = 0;
	};

	struct Route
	{
		std::vector<RoutePoint> routePoints;
		std::vector<RoutePoint> currentSegmentRoutePoints;

		QPainterPath wholeRoutePath;
		QColor wholeRouteColor = QColor(255, 0, 0);
		double wholeRouteWidth = 15.0;

		std::vector<QPointF> controlLocations;
		QColor controlColor = QColor(0, 0, 255);
		double controlRadius = 10.0;

		QPointF runnerLocation;
		QColor runnerColor = QColor(0, 0, 255);
		double runnerRadius = 10.0;
	};

	// Does the actual drawing using OpenGL.
	class Renderer : protected QOpenGLFunctions
	{

	public:

		bool initialize(VideoDecoder* videoDecoder, QuickRouteReader* quickRouteReader, MapImageReader* mapImageReader, VideoStabilizer* videoStabilizer, InputHandler* inputHandler, Settings* settings);
		bool resizeWindow(int newWidth, int newHeight);
		~Renderer();

		void startRendering(double currentTime, double frameTime, double spareTime, double decoderTime, double stabilizerTime, double encoderTime);
		void uploadFrameData(FrameData* frameData);
		void renderAll();
		void stopRendering();
		void getRenderedFrame(FrameData* frameData);

		Panel* getVideoPanel();
		Panel* getMapPanel();
		RenderMode getRenderMode() const;

		void setRenderMode(RenderMode mode);
		void setFlipOutput(bool value);
		void setIsEncoding(bool value);
		void toggleShowInfoPanel();
		void requestFullClear();

	private:

		bool loadShaders(Panel* panel, const QString& shaderName);
		void loadBuffer(Panel* panel, GLfloat* buffer, size_t size);
		void initializeRoute(Route* route, const std::vector<RoutePoint>& routePoints);
		void renderVideoPanel();
		void renderMapPanel();
		void renderInfoPanel();
		void renderPanel(Panel* panel);
		void renderRoute(Route* route);

		VideoStabilizer* videoStabilizer = nullptr;
		InputHandler* inputHandler = nullptr;

		bool shouldFlipOutput = false;
		bool isEncoding = false;
		bool showInfoPanel = false;
		bool fullClearRequested = true;

		double windowWidth = 0.0;
		double windowHeight = 0.0;
		double currentTime = 0.0;
		double frameTime = 0.0;
		int multisamples = 0;

		Panel videoPanel;
		Panel mapPanel;
		RenderMode renderMode = RenderMode::ALL;

		Route defaultRoute;

		QElapsedTimer renderTimer;
		double lastRenderTime = 0.0;

		MovingAverage averageFps;
		MovingAverage averageFrameTime;
		MovingAverage averageDecodeTime;
		MovingAverage averageStabilizeTime;
		MovingAverage averageRenderTime;
		MovingAverage averageEncodeTime;
		MovingAverage averageSpareTime;

		QOpenGLPaintDevice* paintDevice = nullptr;
		QPainter* painter = nullptr;

		QOpenGLFramebufferObject* outputFramebuffer = nullptr;
		QOpenGLFramebufferObject* outputFramebufferNonMultisample = nullptr;
		FrameData renderedFrameData;
	};
}
