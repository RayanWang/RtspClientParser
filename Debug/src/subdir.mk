################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/DataSink.cpp \
../src/RtspParser.cpp \
../src/strmRTSPClient.cpp 

OBJS += \
./src/DataSink.o \
./src/RtspParser.o \
./src/strmRTSPClient.o 

CPP_DEPS += \
./src/DataSink.d \
./src/RtspParser.d \
./src/strmRTSPClient.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -D__GXX_EXPERIMENTAL_CXX0X__ -I../../Live555/src/BasicUsageEnvironment/include -I../../Live555/src/groupsock/include -I../../Live555/src/liveMedia/include -I../../Live555/src/UsageEnvironment/include -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


