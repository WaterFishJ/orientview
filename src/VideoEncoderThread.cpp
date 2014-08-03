// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include "VideoEncoderThread.h"
#include "VideoDecoder.h"
#include "VideoEncoder.h"
#include "RenderOffScreenThread.h"

using namespace OrientView;

VideoEncoderThread::VideoEncoderThread()
{
}

bool VideoEncoderThread::initialize(VideoDecoder* videoDecoder, VideoEncoder* videoEncoder, RenderOffScreenThread* renderOffScreenThread)
{
	qDebug("Initializing VideoEncoderThread");

	this->videoEncoder = videoEncoder;
	this->renderOffScreenThread = renderOffScreenThread;

	totalFrameCount = videoDecoder->getVideoInfo().totalFrameCount;

	return true;
}

void VideoEncoderThread::shutdown()
{
	qDebug("Shutting down VideoEncoderThread");
}

void VideoEncoderThread::run()
{
	FrameData renderedFrameData;

	while (!isInterruptionRequested())
	{
		if (renderOffScreenThread->tryGetNextFrame(&renderedFrameData, 100))
		{
			videoEncoder->loadFrameData(&renderedFrameData);
			renderOffScreenThread->signalFrameRead();
			int frameSize = videoEncoder->encodeFrame();

			emit frameProcessed(renderedFrameData.number, frameSize);

			if (renderedFrameData.number >= totalFrameCount)
				break;
		}
	}

	videoEncoder->close();
	emit encodingFinished();
}
