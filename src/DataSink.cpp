/*
 * DataSink.cpp
 *
 *  Created on: Dec 23, 2014
 *      Author: rayan
 */

#include "DataSink.h"
#include "SPropParameterSetParser.h"
#include <memory>

#define DATA_SINK_VIDEO_RECEIVE_BUFFER_SIZE 1048576	// 1024KB (for TCP used),  ffmpeg has a socket buffer default of 64KB (UDP)

enum FrameFormat {
	SPS_Data	= 0x7,
	PPS_Data	= 0x8,
	I_Frame 	    = 0x5,
	P_Frame 	= 0x1,
};

static const size_t startCodeLength = 4;
static const char startCode[startCodeLength] = {0x00, 0x00, 0x00, 0x01};

inline FrameInfo* FrameNew(int frame_size = 4096)
{
	FrameInfo* frame = (FrameInfo*)malloc(sizeof(FrameInfo)+frame_size);
	if (frame == NULL)
		return(NULL);
	frame->pdata = (char *)frame + sizeof(FrameInfo);
	frame->frameHead.FrameLen = frame_size;
	return(frame);
}

DataSink* DataSink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId) {
  return new DataSink(env, subsession, streamId);
}

DataSink::DataSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env),
	m_Subsession(subsession) {
	m_pStreamId = strDup(streamId);
	m_pReceiveBuffer = new unsigned char[DATA_SINK_VIDEO_RECEIVE_BUFFER_SIZE];
	memset(&m_mediainfo, 0, sizeof(MediaInfo));

	m_sps = m_Subsession.fmtp_spropparametersets();

	if( !strcmp( m_Subsession.mediumName(), "video" ) ) {
		m_mediainfo.codecType 	= CODEC_TYPE_VIDEO;
		m_mediainfo.i_format		= FOURCC('u','n','d','f');
		m_mediainfo.video.fps 		= m_Subsession.videoFPS();
		m_mediainfo.video.height	= m_Subsession.videoHeight();
		m_mediainfo.video.width	= m_Subsession.videoWidth();

		// Now, just H264
		if( !strcmp( m_Subsession.codecName(), "H264" ) ) {
			unsigned int num = 0;
			std::unique_ptr<SPropRecord[]> pSpsRecord(
					parseSPropParameterSets(m_sps, num));

			int extraSpsSize = 0;
			for (unsigned int i = 0; i < num; ++i)
				extraSpsSize += (startCodeLength + pSpsRecord[i].sPropLength);
			m_mediainfo.extraSPS = new char[extraSpsSize + 32];

			for (unsigned int i = 0; i < num; ++i) {
				memcpy(&m_mediainfo.extraSPS[m_mediainfo.extraSPS_Len],
						startCode, startCodeLength);
				m_mediainfo.extraSPS_Len += startCodeLength;

				memcpy(&m_mediainfo.extraSPS[m_mediainfo.extraSPS_Len],
						pSpsRecord[i].sPropBytes, pSpsRecord[i].sPropLength);
				m_mediainfo.extraSPS_Len += pSpsRecord[i].sPropLength;
			}

			m_mediainfo.i_format = FOURCC('h', '2', '6', '4');
			m_mediainfo.b_packetized = false;

			CSPropParameterSetParser spsParser(m_sps);
			m_mediainfo.video.height = spsParser.GetHeight();
			m_mediainfo.video.width = spsParser.GetWidth();
		}
	}
}

DataSink::~DataSink() {
	safeArrayDelete(m_pReceiveBuffer);
	safeArrayDelete(m_pStreamId);
	safeArrayDelete(m_mediainfo.extraSPS);
}

void DataSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
	DataSink* sink = (DataSink*)clientData;
	if( frameSize > DATA_SINK_VIDEO_RECEIVE_BUFFER_SIZE ) {
		;//err("buffer overflow" );
	}
	do {
		if (sink->m_mediainfo.i_format == FOURCC('h', '2', '6', '4')) {
			FrameInfo *p_block;

			static bool hasRecvFirst_I_Frame = false;
			unsigned char type = (sink->m_pReceiveBuffer[0] & 0x1f);	// check the first 5 bits
			if (!hasRecvFirst_I_Frame && I_Frame == type)
				hasRecvFirst_I_Frame = true;
			if (hasRecvFirst_I_Frame && (I_Frame == type || P_Frame == type)) {
				p_block = FrameNew(frameSize + startCodeLength);
				memcpy(p_block->pdata, startCode, startCodeLength);
				memcpy(&p_block->pdata[startCodeLength], sink->m_pReceiveBuffer,
						frameSize);

				unsigned long i_pts = presentationTime.tv_sec * 1000
						+ presentationTime.tv_usec / 1000;

				p_block->frameHead.FrameType =
						(long) (sink->m_mediainfo.codecType);
				p_block->frameHead.TimeStamp = i_pts;
				p_block->frameHead.frameDuration = durationInMicroseconds;
				sink->m_frameQueue.put(p_block);
			}
		}
	} while(false);
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
//#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void DataSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
	// We've just received a frame of data.  (Optionally) print out information about it:
#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
	if (m_pStreamId != NULL)
		envir() << "Stream \"" << m_pStreamId << "\"; ";
	envir() << m_Subsession.mediumName() << "/" << m_Subsession.codecName() << ":\tReceived " << frameSize << " bytes";
	if (numTruncatedBytes > 0)
		envir() << " (with " << numTruncatedBytes << " bytes truncated)";

	char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
	sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
	envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;

	if (m_Subsession.rtpSource() != NULL && !m_Subsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
		envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
	}
#ifdef DEBUG_PRINT_NPT
	envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
	envir() << "\n";
#endif

	// Then continue, to request the next frame of data:
	continuePlaying();
}

Boolean DataSink::continuePlaying() {
	if (fSource == NULL)
		return False; // sanity check (should not happen)

	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(m_pReceiveBuffer, DATA_SINK_VIDEO_RECEIVE_BUFFER_SIZE,
                        	afterGettingFrame, this,
							onSourceClosure, this);
	return True;
}
