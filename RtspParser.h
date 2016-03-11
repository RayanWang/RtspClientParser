/*
 * RtspParser.h
 *
 *  Created on: Dec 22, 2014
 *      Author: rayan
 */

#ifndef SRC_RTSPPARSER_H_
#define SRC_RTSPPARSER_H_

typedef int (*fDataReadyCallback)(long lMainEventType, long lSubEventType, void* pReserved, void* pData);

enum MediaType {
    MEDIA_TYPE_UNKNOWN = -1,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_AUDIO
};

// callback main event
enum eventCB {
	cb_dataReady = 0,
	cb_receiveNoData,
	cb_prepareReconnect,
} eventCB;

typedef struct FrameData {
	long TimeStamp;
	long frameDuration;
	long FrameType;
	long FrameLen;
	char* pdata;
};

typedef struct MediaData {
	int		width;
	int		height;
	char*	extraSPS;
	int		extraSPS_Len;
};

class CRtspParser {
public:
	CRtspParser();
	virtual ~CRtspParser();

public:
	int rtspClientOpenAndPlay(char const* progName, char const* rtspURL, int nIpcID, void* pStreamObj);
	int rtspClientReadFrame(int nIpcID, FrameData* pFrameData);
	int rtspClientCloseAllStream(void);
	int rtspClientCloseStream(int nIpcID);
	int rtspClinetGetMediaInfo(int nIpcID, MediaData& mediaData);
	void setCallback(int nIpcID, void* pStreamObj, fDataReadyCallback Callback);
};

#endif /* SRC_RTSPPARSER_H_ */
