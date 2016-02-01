/*
 * strmRTSPClient.cpp
 *
 *  Created on: Dec 23, 2014
 *      Author: rayan
 */

#include "strmRTSPClient.h"

strmRTSPClient* strmRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL, int nIpcID, void* pStreamObj,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new strmRTSPClient(env, rtspURL, nIpcID, pStreamObj, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

strmRTSPClient::strmRTSPClient(UsageEnvironment& env, char const* rtspURL, int nIpcID, void* pStreamObj,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1)
  , m_nIpcID(nIpcID)
  , m_pStreamObj(pStreamObj)
  /*, m_eventLoopWatchVariable(0)*/{
}

strmRTSPClient::~strmRTSPClient() {
}
