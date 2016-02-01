/*
 * strmRTSPClient.h
 *
 *  Created on: Dec 23, 2014
 *      Author: rayan
 */

#ifndef SRC_STRMRTSPCLIENT_H_
#define SRC_STRMRTSPCLIENT_H_

#include "common.h"
#include "pthread.h"

class StreamClientState {
public:
	StreamClientState(){
		iter = NULL;
		m_pSession = NULL;
		m_pSubsession = NULL;
		m_streamTimerTask = NULL;
		m_duration = 0.0;
	}
	virtual ~StreamClientState() {
		safeDelete(iter);
		if(m_pSession) {
			UsageEnvironment& env = m_pSession->envir();
			env.taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
			Medium::close(m_pSession);
		}
	}

public:
	MediaSubsessionIterator* 	iter;
	MediaSession* 				m_pSession;
	MediaSubsession* 			m_pSubsession;
	TaskToken 					m_streamTimerTask;
	TaskToken 					m_arrivalCheckTimerTask;
	double 						m_duration;
};

class strmRTSPClient: public RTSPClient {
public:
	static strmRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL, int nIpcID, void* pStreamObj,
				  	  int verbosityLevel = 0,
					  char const* applicationName = NULL,
					  portNumBits tunnelOverHTTPPortNum = 0);

protected:
	strmRTSPClient(UsageEnvironment& env, char const* rtspURL, int nIpcID, void* pStreamObj,
			int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
	virtual ~strmRTSPClient();

public:
	StreamClientState 	m_scs;
	int					m_nIpcID;
	void* 				m_pStreamObj;
	//char 				m_eventLoopWatchVariable;
};



#endif /* SRC_STRMRTSPCLIENT_H_ */
