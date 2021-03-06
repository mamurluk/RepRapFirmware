/*
 * RtdSensor31865.h
 *
 *  Created on: 8 Jun 2017
 *      Author: David
 */

#ifndef SRC_HEATING_RTDSENSOR31865_H_
#define SRC_HEATING_RTDSENSOR31865_H_

#include "SpiTemperatureSensor.h"

class RtdSensor31865 : public SpiTemperatureSensor
{
public:
	RtdSensor31865(unsigned int sensorNum) noexcept;
	GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply) override;

	static constexpr const char *TypeName = "rtdmax31865";

	void Poll() noexcept override;

private:
	TemperatureError TryInitRtd() const noexcept;

	uint16_t rref;				// reference resistor in ohms
	uint8_t cr0;
};

#endif /* SRC_HEATING_RTDSENSOR31865_H_ */
