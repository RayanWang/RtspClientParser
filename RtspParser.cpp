/*
 * RtspParser.cpp
 *
 *  Created on: Dec 22, 2014
 *      Author: rayan
 */

#include <unistd.h>
#include <map>

#include "common.h"
#include "DataSink.h"
#include "RtspParser.h"
#include "strmRTSPClient.h"

using namespace std;

class strmRTSPClient;

struct RtspData{
	strmRTSPClient* 	rtspClient;
	DataSink*				sink;
	char 						eventLoopWatchVariable;
	bool							bReconnect;
	int							nReconnectCount;

	unsigned 				prevTotNumPacketsReceived;
	unsigned 				currTotNumPacketsReceived;
	int 							timeWithNoData;
};

unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.
fDataReadyCallback g_pfCallback = NULL;
map<int, RtspData> g_mapRtspData;

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

void checkForPacketArrival(RTSPClient* rtspClient);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
	return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
	return env << subsession.mediumName() << "/" << subsession.codecName();
}

// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
			delete[] resultString;
			break;
		}

		char* sdpDescription = resultString;
		env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

		// Create a media session object from this SDP description:
		scs.m_pSession = MediaSession::createNew(env, sdpDescription);
		safeArrayDelete(sdpDescription);

		if (scs.m_pSession == NULL) {
			env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
			break;
		}
		else if (!scs.m_pSession->hasSubsessions()) {
			env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
			break;
		}

		// Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
		// calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
		// (Each 'subsession' will have its own data source.)
		scs.iter = new MediaSubsessionIterator(*scs.m_pSession);
		setupNextSubsession(rtspClient);

		return;
	} while (0);

	// An unrecoverable error occurred with this stream.
	shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True

void setupNextSubsession(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

	scs.m_pSubsession = scs.iter->next();
	if (scs.m_pSubsession != NULL) {
		if (!scs.m_pSubsession->initiate()) {
			env << *rtspClient << "Failed to initiate the \"" << *scs.m_pSubsession << "\" subsession: " << env.getResultMsg() << "\n";
			setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
		}
		else {
			env << *rtspClient << "Initiated the \"" << *scs.m_pSubsession << "\" subsession (";
			if (scs.m_pSubsession->rtcpIsMuxed()) {
				env << "client port " << scs.m_pSubsession->clientPortNum();
			}
			else {
				env << "client ports " << scs.m_pSubsession->clientPortNum() << "-" << scs.m_pSubsession->clientPortNum()+1;
			}
			env << ")\n";

			// Continue setting up this subsession, by sending a RTSP "SETUP" command:
			rtspClient->sendSetupCommand(*scs.m_pSubsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
		}

		return;
	}

	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	if (scs.m_pSession->absStartTime() != NULL) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		rtspClient->sendPlayCommand(*scs.m_pSession, continueAfterPLAY, scs.m_pSession->absStartTime(), scs.m_pSession->absEndTime());
	}
	else {
		scs.m_duration = scs.m_pSession->playEndTime() - scs.m_pSession->playStartTime();
		rtspClient->sendPlayCommand(*scs.m_pSession, continueAfterPLAY);
	}
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to set up the \"" << *scs.m_pSubsession << "\" subsession: " << resultString << "\n";
			break;
		}

		env << *rtspClient << "Set up the \"" << *scs.m_pSubsession << "\" subsession (";
		if (scs.m_pSubsession->rtcpIsMuxed()) {
			env << "client port " << scs.m_pSubsession->clientPortNum();
		}
		else {
			env << "client ports " << scs.m_pSubsession->clientPortNum() << "-" << scs.m_pSubsession->clientPortNum()+1;
		}
		env << ")\n";

		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		scs.m_pSubsession->sink = DataSink::createNew(env, *scs.m_pSubsession, rtspClient->url());
		// perhaps use your own custom "MediaSink" subclass instead
		if (scs.m_pSubsession->sink == NULL) {
			env << *rtspClient << "Failed to create a data sink for the \"" << *scs.m_pSubsession
					<< "\" subsession: " << env.getResultMsg() << "\n";
			break;
		}

		env << *rtspClient << "Created a data sink for the \"" << *scs.m_pSubsession << "\" subsession\n";

		scs.m_pSubsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession
		scs.m_pSubsession->sink->startPlaying(*(scs.m_pSubsession->readSource()),
						   	   	   	   	   	   subsessionAfterPlaying, scs.m_pSubsession);
		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if (scs.m_pSubsession->rtcpInstance() != NULL) {
			scs.m_pSubsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.m_pSubsession);
		}

		//g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].rtspClient = ((strmRTSPClient*)rtspClient);
		if( !strcmp( scs.m_pSubsession->mediumName(), "video" ) ) {
			g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].sink = (DataSink*)scs.m_pSubsession->sink;

			if (g_pfCallback) {
				g_pfCallback(cb_dataReady, g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].bReconnect, NULL, ((strmRTSPClient*)rtspClient)->m_pStreamObj);
				if (g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].bReconnect)
					g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].bReconnect = false;
			}
		}
	} while (0);
	safeArrayDelete(resultString);

	// Set up the next subsession, if any:
	setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
	Boolean success = False;

	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
			break;
		}

		// Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
		// using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
		// 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
		// (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
		if (scs.m_duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.m_duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.m_duration*1000000);
			scs.m_streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
		}

		env << *rtspClient << "Started playing session";
		if (scs.m_duration > 0) {
			env << " (for up to " << scs.m_duration << " seconds)";
		}
		env << "...\n";

		success = True;
	} while (0);
	safeArrayDelete(resultString);

	if (!success) {
		// An unrecoverable error occurred with this stream.
		shutdownStream(rtspClient);
	}

	checkForPacketArrival(rtspClient);
}

// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
  	subsession->sink = NULL;

  	// Next, check whether *all* subsessions' streams have now been closed:
  	MediaSession& session = subsession->parentSession();
  	MediaSubsessionIterator iter(session);
  	while ((subsession = iter.next()) != NULL) {
  		if (subsession->sink != NULL)
  			return; // this subsession is still active
  	}

  	// All subsessions' streams have now been closed, so shutdown the client:
  	shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
	UsageEnvironment& env = rtspClient->envir(); // alias

	env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

	// Now act as if the subsession had closed:
	subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
	strmRTSPClient* rtspClient = (strmRTSPClient*)clientData;
	StreamClientState& scs = rtspClient->m_scs; // alias

	scs.m_streamTimerTask = NULL;

	// Shut down the stream:
	shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

	// First, check whether any subsessions have still to be closed:
	if (scs.m_pSession != NULL) {
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.m_pSession);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;

				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				}

				someSubsessionsWereActive = True;
			}
		}

		if (someSubsessionsWereActive) {
			// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
			// Don't bother handling the response to the "TEARDOWN".
			rtspClient->sendTeardownCommand(*scs.m_pSession, NULL);
		}
	}

	g_mapRtspData[((strmRTSPClient*)rtspClient)->m_nIpcID].eventLoopWatchVariable = 0xff;
	--rtspClientCount;

	env << *rtspClient << "Closing the stream.\n";
	Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
}

void checkForPacketArrival(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((strmRTSPClient*)rtspClient)->m_scs; // alias

	MediaSubsessionIterator iter(*scs.m_pSession);
	MediaSubsession* subsession;
	int nIpcID = ((strmRTSPClient*) rtspClient)->m_nIpcID;
	while ((subsession = iter.next()) != NULL) {
		RTPSource* src = subsession->rtpSource();
		if (src == NULL)
			continue;
		if (!strcmp(subsession->mediumName(), "video"))
			g_mapRtspData[nIpcID].currTotNumPacketsReceived =
					src->receptionStatsDB().totNumPacketsReceived();
	}

	if (g_mapRtspData[nIpcID].currTotNumPacketsReceived
			> g_mapRtspData[nIpcID].prevTotNumPacketsReceived) {
		g_mapRtspData[nIpcID].prevTotNumPacketsReceived = g_mapRtspData[nIpcID].currTotNumPacketsReceived;
		g_mapRtspData[nIpcID].timeWithNoData = 0;
		g_mapRtspData[((strmRTSPClient*) rtspClient)->m_nIpcID].nReconnectCount = 0;
	} else {
		g_mapRtspData[nIpcID].timeWithNoData += 100000;
		if (g_mapRtspData[nIpcID].timeWithNoData >= 1000000) {		// 1000ms
			g_mapRtspData[nIpcID].bReconnect = true;
			g_mapRtspData[nIpcID].timeWithNoData = 0;
			g_pfCallback(cb_prepareReconnect, NULL, NULL, ((strmRTSPClient*) rtspClient)->m_pStreamObj);
			shutdownStream(g_mapRtspData[nIpcID].rtspClient);
			return;
		}
	}

	int uSecsToDelay = 100000; // 100 ms
	env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)checkForPacketArrival, rtspClient);
}

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

CRtspParser::CRtspParser() {
	// TODO Auto-generated constructor stub
	g_mapRtspData.clear();
}

CRtspParser::~CRtspParser() {
	// TODO Auto-generated destructor stub
	g_mapRtspData.clear();
}

int CRtspParser::rtspClientOpenAndPlay(char const* progName, char const* rtspURL, int nIpcID, void* pStreamObj) {
	// Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
	// to receive (even if more than stream uses the same "rtsp://" URL).
	TaskScheduler* pScheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* pEnv = BasicUsageEnvironment::createNew(*pScheduler);

	strmRTSPClient* rtspClient = strmRTSPClient::createNew(*pEnv, rtspURL, nIpcID, pStreamObj,
														   RTSP_CLIENT_VERBOSITY_LEVEL, progName);
	if (rtspClient == NULL) {
		*pEnv << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << pEnv->getResultMsg() << "\n";
	    return (-1);
	}

	++rtspClientCount;

	RtspData rtsp;
	memset(&rtsp, 0, sizeof(RtspData));
	rtsp.rtspClient = rtspClient;

	pair<map<int, RtspData>::iterator, bool> insertresult;
	insertresult = g_mapRtspData.insert(pair<int, RtspData>(nIpcID, rtsp));
	if(false == insertresult.second) {
		g_mapRtspData[nIpcID] = rtsp;
	}

	// Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
	// Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
	// Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
	rtspClient->sendDescribeCommand(continueAfterDESCRIBE);

	pEnv->taskScheduler().doEventLoop(&g_mapRtspData[nIpcID].eventLoopWatchVariable);

	map<int, RtspData>::iterator it;
	while (g_mapRtspData[nIpcID].bReconnect) {
		int tmp = g_mapRtspData[nIpcID].nReconnectCount;

		if ((it = g_mapRtspData.find(nIpcID)) != g_mapRtspData.end())
			g_mapRtspData.erase(it);

		// Re-init
		pScheduler = BasicTaskScheduler::createNew();
		pEnv = BasicUsageEnvironment::createNew(*pScheduler);
		rtspClient = strmRTSPClient::createNew(*pEnv, rtspURL, nIpcID, pStreamObj, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
		if (rtspClient == NULL) {
			*pEnv << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << pEnv->getResultMsg() << "\n";
			return (-1);
		}

		++rtspClientCount;

		memset(&rtsp, 0, sizeof(RtspData));
		rtsp.rtspClient = rtspClient;
		rtsp.nReconnectCount = ++tmp;
		rtsp.bReconnect = true;

		g_mapRtspData.insert(pair<int, RtspData>(nIpcID, rtsp));

		// Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
		// Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
		// Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
		rtspClient->sendDescribeCommand(continueAfterDESCRIBE);

		pEnv->taskScheduler().doEventLoop(&g_mapRtspData[nIpcID].eventLoopWatchVariable);

		if(g_mapRtspData[nIpcID].nReconnectCount >= 10) {		// Retry connection to rtsp server about 10 times
			if (g_pfCallback) {
				g_mapRtspData[nIpcID].bReconnect = false;
				g_pfCallback(cb_receiveNoData, NULL, NULL,
						((strmRTSPClient*) rtspClient)->m_pStreamObj);
			}
		}
	}

	if ((it = g_mapRtspData.find(nIpcID)) != g_mapRtspData.end()) {
		g_mapRtspData.erase(it);
	}

	return 0;
}

int CRtspParser::rtspClientReadFrame(int nIpcID, FrameData* pFrameData) {
	DataSink* sink = g_mapRtspData[nIpcID].sink;
	FrameInfo* frame = sink->m_frameQueue.get();

	if (pFrameData && frame && frame->pdata) {
		if (NULL == pFrameData->pdata)
			pFrameData->pdata = new char[frame->frameHead.FrameLen];
		memcpy(pFrameData->pdata, frame->pdata, frame->frameHead.FrameLen);
		pFrameData->FrameLen = frame->frameHead.FrameLen;
		pFrameData->FrameType = frame->frameHead.FrameType;
		pFrameData->TimeStamp = frame->frameHead.TimeStamp;

		free(frame);
		return(pFrameData->FrameLen);
	}

	return -1;
}

int CRtspParser::rtspClientCloseAllStream(void) {
	map<int, RtspData>::iterator it;
	for(it = g_mapRtspData.begin(); it != g_mapRtspData.end(); it++) {
		if(it->second.sink) {
			it->second.sink->m_frameQueue.empty();
		}
		shutdownStream(it->second.rtspClient);
	}

	return 0;
}

int CRtspParser::rtspClientCloseStream(int nIpcID) {
	if(g_mapRtspData[nIpcID].sink) {
		g_mapRtspData[nIpcID].sink->m_frameQueue.empty();
	}
	shutdownStream(g_mapRtspData[nIpcID].rtspClient);

	return 0;
}

int CRtspParser::rtspClinetGetMediaInfo(int nIpcID, MediaData& mediaData) {
	DataSink* sink = g_mapRtspData[nIpcID].sink;
	if (sink) {
		mediaData.width = sink->m_mediainfo.video.width;
		mediaData.height = sink->m_mediainfo.video.height;
		mediaData.extraSPS = sink->m_mediainfo.extraSPS;
		mediaData.extraSPS_Len = sink->m_mediainfo.extraSPS_Len;
		return 0;
	}
	return (-1);
}

void CRtspParser::setCallback(int nIpcID, void* pStreamObj, fDataReadyCallback Callback) {
	g_pfCallback = Callback;
}
