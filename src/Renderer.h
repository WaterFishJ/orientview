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

#include "MovingAverage.h"

namespace OrientView
{
	class VideoWindow;
	class VideoDecoder;
	class QuickRouteReader;
	class MapImageReader;
	class VideoStabilizer;
	class InputHandler;
	class Settings;
	struct FrameData;

	enum class RenderMode { ALL, VIDEO, MAP };

	struct Panel
	{
		QOpenGLShaderProgram* program = nullptr;
		QOpenGLBuffer* buffer = nullptr;
		QOpenGLTexture* texture = nullptr;

		QMatrix4x4 vertexMatrix;

		QColor clearColor = QColor(0, 0, 0);
		bool clearEnabled = true;

		double textureWidth = 0.0;
		double textureHeight = 0.0;
		double texelWidth = 0.0;
		double texelHeight = 0.0;

		double x = 0.0;
		double y = 0.0;
		double angle = 0.0;
		double scale = 1.0;
		double userX = 0.0;
		double userY = 0.0;
		double userAngle = 0.0;
		double userScale = 1.0;

		GLuint vertexMatrixUniform = 0;
		GLuint vertexPositionAttribute = 0;
		GLuint vertexTextureCoordinateAttribute = 0;
		GLuint textureSamplerUniform = 0;
		GLuint textureWidthUniform = 0;
		GLuint textureHeightUniform = 0;
		GLuint texelWidthUniform = 0;
		GLuint texelHeightUniform = 0;
	};

	class Renderer : protected QOpenGLFunctions
	{

	public:

		Renderer();

		bool initialize(VideoWindow* videoWindow, VideoDecoder* videoDecoder, QuickRouteReader* quickRouteReader, MapImageReader* mapImageReader, VideoStabilizer* videoStabilizer, InputHandler* inputHandler, Settings* settings);
		void shutdown();

		void startRendering(double windowWidth, double windowHeight, double frameTime, double spareTime, double decoderTime, double stabilizerTime, double encoderTime);
		void uploadFrameData(FrameData* frameData);
		void renderAll();
		void stopRendering();

		Panel* getVideoPanel();
		Panel* getMapPanel();
		RenderMode getRenderMode();

		void setRenderMode(RenderMode mode);
		void setFlipOutput(bool value);
		void toggleShowInfoPanel();

	private:

		bool loadShaders(Panel* panel, const QString& shaderName);
		void loadBuffer(Panel* panel, GLfloat* buffer, int size);
		void renderVideoPanel();
		void renderMapPanel();
		void renderInfoPanel();
		void renderPanel(Panel* panel);
		void renderRoute();

		VideoStabilizer* videoStabilizer = nullptr;
		InputHandler* inputHandler = nullptr;

		bool flipOutput = false;
		bool showInfoPanel = false;

		double mapPanelRelativeWidth = 0.0;
		double windowWidth = 0.0;
		double windowHeight = 0.0;
		double frameTime = 0.0;

		Panel videoPanel;
		Panel mapPanel;
		RenderMode renderMode = RenderMode::ALL;

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
		QPainterPath* routePath = nullptr;
	};
}
