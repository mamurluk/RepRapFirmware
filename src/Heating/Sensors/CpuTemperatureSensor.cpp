/*
 * CpuTemperatureSensor.cpp
 *
 *  Created on: 8 Jun 2017
 *      Author: David
 */

#include "CpuTemperatureSensor.h"
#include "Platform.h"
#include "RepRap.h"

#if HAS_CPU_TEMP_SENSOR

CpuTemperatureSensor::CpuTemperatureSensor(unsigned int sensorNum) noexcept : TemperatureSensor(sensorNum, "microcontroller embedded temperature sensor")
{
}

void CpuTemperatureSensor::Poll() noexcept
{
	float minT, currentT, maxT;
	reprap.GetPlatform().GetMcuTemperatures(minT, currentT, maxT);
	SetResult(currentT, TemperatureError::success);
}

#endif

// End
