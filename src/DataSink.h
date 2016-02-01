/*
 * DataSink.h
 *
 *  Created on: Dec 23, 2014
 *      Author: rayan
 */

#ifndef SRC_DATASINK_H_
#define SRC_DATASINK_H_

#include <pthread.h>

#include "common.h"

typedef unsigned int  	fourcc_t;
#define FOURCC( a, b, c, d ) ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )

//#define MAX_SPS_LENGTH 100

enum CodecType
{
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
    CODEC_TYPE_DATA,
    CODEC_TYPE_SUBTITLE,
    CODEC_TYPE_ATTACHMENT,
    CODEC_TYPE_NB
};

typedef struct __FrameHeader
{
	long TimeStamp;
	long frameDuration;
	long FrameType;
	long FrameLen;
} FrameHeader;


typedef struct __FrameInfo
{
    FrameHeader frameHead;
	char* pdata;
} FrameInfo;

typedef struct __MediaQueue
{
	FrameInfo *frame;
	struct __MediaQueue *next;
} MediaQueue;

typedef struct __AudioFormat
{
	uint32_t	sampleRate;
	uint32_t	channels;
	uint32_t	i_bytes_per_frame;
	uint32_t	i_frame_length;
	uint32_t	i_bitspersample;
} AudioFormat;

typedef struct __VideoFormat
{
	int	width;
	int	height;
	int	fps;
	int bitrate;
} VideoFormat;

typedef struct __MediaInfo
{
	enum CodecType 	codecType;
	fourcc_t  				i_format;
	char 						codecName[50];
	AudioFormat 		audio;
	VideoFormat 		video;
	int						duration;
	int 						b_packetized;
	char*						extraSPS;
	int						extraSPS_Len;
} MediaInfo;

class CMediaQueue
{
#define  MAX_FRAME_COUNT 1800
	MediaQueue *head, *tail;
	MediaQueue *writepos, *readpos;
	MediaQueue *ptr, *pstatic;

	int isLive;

	pthread_mutex_t m_FrameListLock;
	pthread_mutex_t m_RecvMutex;
	pthread_cond_t 	m_RecvCond;

public:
	int  count;

	CMediaQueue() {
		head = tail = NULL;
		ptr = pstatic = (MediaQueue *) malloc(sizeof(MediaQueue) * MAX_FRAME_COUNT);
		memset(pstatic, 0, sizeof(MediaQueue)*MAX_FRAME_COUNT);
		head = ptr;
		readpos = writepos = head;
		for (int i = 1; i < MAX_FRAME_COUNT; i++) {
			ptr->next = pstatic + i;
			ptr = ptr->next;
		}
		tail = ptr;
		tail->next =  head;
		ptr = head;
		count = 0;
		isLive = 1;
		pthread_mutex_init(&m_FrameListLock, NULL);
		pthread_mutex_init(&m_RecvMutex, NULL);
		pthread_cond_init(&m_RecvCond, NULL);
	}

	~CMediaQueue() {
		pthread_mutex_destroy(&m_RecvMutex);
		pthread_cond_destroy(&m_RecvCond);
		pthread_mutex_destroy(&m_FrameListLock);

		ptr = head;
		for (int i = 0; i < MAX_FRAME_COUNT; i++) {
			if(ptr) {
				if(ptr->frame) {
					free(ptr->frame);
					ptr->frame =  NULL;
				}
				ptr = ptr->next;
			}
		}
		free(pstatic);
	}

	void put(FrameInfo* frame) {
		if(ptr == NULL)
			return;

		pthread_mutex_lock(&m_FrameListLock);
		if (count >= MAX_FRAME_COUNT) {
			count = MAX_FRAME_COUNT - 1;
			if (readpos->frame) {
				free(readpos->frame);
				readpos->frame = NULL;
			}
			readpos = readpos->next;
		}
		writepos->frame = frame;
		writepos = writepos->next;
		count++;

		if (count <= 1) {
			pthread_cond_signal(&m_RecvCond);
		}
		pthread_mutex_unlock(&m_FrameListLock);
	}

	FrameInfo* get() {
		FrameInfo* frame = NULL;

		pthread_mutex_lock(&m_RecvMutex);
		if (count < 1) {
			struct timespec outtime;
			if (isLive == 0) {
				get_abstime_wait(8000, &outtime);
			}
			else {
				get_abstime_wait(60000, &outtime);
			}
			pthread_cond_timedwait(&m_RecvCond, &m_RecvMutex, &outtime);
		}
		pthread_mutex_unlock(&m_RecvMutex);

		pthread_mutex_lock(&m_FrameListLock);
		if(count > 0) {
			frame = readpos->frame;
			readpos->frame =  NULL;
			readpos = readpos->next;
			--count;
		}
		pthread_mutex_unlock(&m_FrameListLock);
		return(frame);
	}

	int size() {
		return(count);
	}

	int isempty() {
		return ( (count <= 0 ) ? 1 : 0 );
	}

	int empty() {
		pthread_cond_signal(&m_RecvCond);
		return count = 0;
	}

	void setLive() {
		isLive = 1;
	}

	void clearLive() {
		isLive = 0;
	}

	int getLive() {
		return isLive;
	}

	void reset() {
		pthread_mutex_lock(&m_FrameListLock);
		ptr = readpos;
		while (ptr->frame) {
			free(ptr->frame);
			ptr->frame = NULL;
			ptr = ptr->next;
		}
		writepos = readpos;
		count  = 0;
		pthread_mutex_unlock(&m_FrameListLock);
	}

private:
	void get_abstime_wait(int milliseconds, struct timespec *abstime) {
		struct timeval tv;
		long long absmsec;
		gettimeofday(&tv, NULL);
		absmsec = tv.tv_sec * 1000ll + tv.tv_usec/1000ll;
		absmsec += milliseconds;
		abstime->tv_sec = absmsec / 1000ll;
		abstime->tv_nsec = absmsec % 1000ll * 1000000ll;
	}
};

class DataSink: public MediaSink {
public:
	static DataSink* createNew(UsageEnvironment& env,
					  	  	  	  MediaSubsession& subsession,  // identifies the kind of data that's being received
								  char const* streamId = NULL); // identifies the stream itself (optional)

private:
	DataSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
    // called only by "createNew()"
	virtual ~DataSink();

	static void afterGettingFrame(void* clientData, unsigned frameSize,
									unsigned numTruncatedBytes,
									struct timeval presentationTime,
									unsigned durationInMicroseconds);
	void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				 	 	 	 struct timeval presentationTime, unsigned durationInMicroseconds);

private:
	// redefined virtual functions:
	virtual Boolean continuePlaying();

public:
	MediaInfo   		m_mediainfo;
	CMediaQueue	m_frameQueue;

private:
	unsigned char*			m_pReceiveBuffer;
	MediaSubsession& 	m_Subsession;
	char* 						m_pStreamId;
	char const*				m_sps;
};

#endif /* SRC_DATASINK_H_ */
