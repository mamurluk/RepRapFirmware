/*
 * ThermocoupleSensor31855.h
 *
 *  Created on: 8 Jun 2017
 *      Author: David
 */

#ifndef SRC_HEATING_THERMOCOUPLESENSOR31855_H_
#define SRC_HEATING_THERMOCOUPLESENSOR31855_H_

#include "SpiTemperatureSensor.h"

class ThermocoupleSensor31855 : public SpiTemperatureSensor
{
public:
	ThermocoupleSensor31855(unsigned int sensorNum) noexcept;
	GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply) override;

	static constexpr const char *TypeName = "thermocouplemax31855";

	void Poll() noexcept override;
};

#endif /* SRC_HEATING_THERMOCOUPLESENSOR31855_H_ */
