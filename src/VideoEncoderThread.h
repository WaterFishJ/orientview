// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QThread>

namespace OrientView
{
	class VideoDecoder;
	class VideoEncoder;
	class RenderOffScreenThread;

	class VideoEncoderThread : public QThread
	{
		Q_OBJECT

	public:

		bool initialize(VideoDecoder* videoDecoder, VideoEncoder* videoEncoder, RenderOffScreenThread* renderOffScreenThread);
		void shutdown();

	signals:

		void frameProcessed(int frameNumber, int frameSize);
		void encodingFinished();

	protected:

		void run();

	private:

		VideoDecoder* videoDecoder = nullptr;
		VideoEncoder* videoEncoder = nullptr;
		RenderOffScreenThread* renderOffScreenThread = nullptr;
	};
}
