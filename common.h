/*
 * common.h
 *
 *  Created on: Dec 23, 2014
 *      Author: rayan
 */

#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <stddef.h>

#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "liveMedia.hh"

#define safeDelete(p) if(p) { delete p; p = NULL; }
#define safeArrayDelete(p) if(p) { delete[] p; p = NULL; }
#define checkPointer(p, ret) if(NULL == p) { return ret; }

#endif /* SRC_COMMON_H_ */
