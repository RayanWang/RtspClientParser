################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../DataSink.cpp \
../RtspParser.cpp \
../strmRTSPClient.cpp 

OBJS += \
./DataSink.o \
./RtspParser.o \
./strmRTSPClient.o 

CPP_DEPS += \
./DataSink.d \
./RtspParser.d \
./strmRTSPClient.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I"/home/rayan/workspace/Live555/src/BasicUsageEnvironment/include" -I"/home/rayan/workspace/Live555/src/groupsock/include" -I"/home/rayan/workspace/Live555/src/liveMedia/include" -I"/home/rayan/workspace/Live555/src/UsageEnvironment/include" -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


