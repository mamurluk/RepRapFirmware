/*
 * RemoteFan.h
 *
 *  Created on: 3 Sep 2019
 *      Author: David
 */

#ifndef SRC_FANS_REMOTEFAN_H_
#define SRC_FANS_REMOTEFAN_H_

#include "Fan.h"

#if SUPPORT_CAN_EXPANSION

#include "GCodes/GCodeResult.h"

class RemoteFan : public Fan
{
public:
	RemoteFan(unsigned int fanNum, CanAddress boardNum) noexcept;
	~RemoteFan() noexcept;

	bool Check() noexcept override;									// update the fan PWM returning true if it is a thermostatic fan that is on
	bool IsEnabled() const noexcept override;
	GCodeResult SetPwmFrequency(PwmFrequency freq, const StringRef& reply) noexcept override;
	int32_t GetRPM() noexcept override;
	GCodeResult ReportPortDetails(const StringRef& str) const noexcept override;
	void UpdateRpmFromRemote(CanAddress src, int32_t rpm) noexcept override;

	GCodeResult ConfigurePort(const char *pinNames, PwmFrequency freq, const StringRef& reply) noexcept;

protected:
	bool UpdateFanConfiguration(const StringRef& reply) noexcept override;
	GCodeResult Refresh(const StringRef& reply) noexcept override;

private:
	static constexpr uint32_t RpmReadingTimeout = 2000;		// any reading older than this number of milliseconds is considered unreliable

	int32_t lastRpm;
	uint32_t whenLastRpmReceived;
	CanAddress boardNumber;
	bool thermostaticFanRunning;
};

#endif

#endif /* SRC_FANS_REMOTEFAN_H_ */
