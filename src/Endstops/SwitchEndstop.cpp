/*
 * LocalSwitchEndstop.cpp
 *
 *  Created on: 15 Sep 2019
 *      Author: David
 */

#include <Endstops/SwitchEndstop.h>

#include "RepRap.h"
#include "Platform.h"
#include "Movement/Kinematics/Kinematics.h"
#include "GCodes/GCodeBuffer/GCodeBuffer.h"

#if SUPPORT_CAN_EXPANSION
# include "CanId.h"
# include "CanMessageBuffer.h"
# include "CanMessageFormats.h"
# include "CAN/CanInterface.h"
#endif

// Switch endstop
SwitchEndstop::SwitchEndstop(uint8_t axis, EndStopPosition pos) : Endstop(axis, pos), numPortsUsed(0)
{
	// ports will be initialised automatically by the IoPort default constructor
}

SwitchEndstop::~SwitchEndstop()
{
	ReleasePorts();
}

// Release any local and remote ports we have allocated and set numPortsUsed to zero
void SwitchEndstop::ReleasePorts()
{
	while (numPortsUsed != 0)
	{
		--numPortsUsed;
#if SUPPORT_CAN_EXPANSION
		const CanAddress bn = boardNumbers[numPortsUsed];
		if (bn != CanId::MasterAddress)
		{
			RemoteInputHandle h(RemoteInputHandle::typeEndstop, GetAxis(), numPortsUsed);
			String<StringLength100> reply;
			if (CanInterface::DeleteHandle(bn, h, reply.GetRef()) != GCodeResult::ok)
			{
				reply.cat('\n');
				reprap.GetPlatform().Message(ErrorMessage, reply.c_str());
			}
		}
#endif
		ports[numPortsUsed].Release();
	}
}

GCodeResult SwitchEndstop::Configure(GCodeBuffer& gb, const StringRef& reply)
{
	String<StringLength50> portNames;
	if (!gb.GetReducedString(portNames.GetRef()))
	{
		reply.copy("Missing port name string");
		return GCodeResult::error;
	}

	return Configure(portNames.c_str(), reply);
}

GCodeResult SwitchEndstop::Configure(const char *pinNames, const StringRef& reply)
{
	ReleasePorts();

	// Parse the string into individual port names
	size_t index = 0;
	while (numPortsUsed < MaxDriversPerAxis)
	{
		// Get the next port name
		String<StringLength50> pn;
		char c;
		while ((c = pinNames[index]) != 0 && c != '+')
		{
			pn.cat(c);
			++index;
		}

#if SUPPORT_CAN_EXPANSION
		const CanAddress boardAddress = IoPort::RemoveBoardAddress(pn.GetRef());
		boardNumbers[numPortsUsed] = boardAddress;
		if (boardAddress != CanId::MasterAddress)
		{
			RemoteInputHandle h(RemoteInputHandle::typeEndstop, GetAxis(), numPortsUsed);
			const GCodeResult rslt = CanInterface::CreateHandle(boardAddress, h, pn.c_str(), 0, MinimumSwitchReportInterval, states[numPortsUsed], reply);
			if (rslt != GCodeResult::ok)
			{
				ReleasePorts();
				return rslt;
			}
		}
		else
#endif
		{
			// Try to allocate the port
			if (!ports[numPortsUsed].AssignPort(pn.c_str(), reply, PinUsedBy::endstop, PinAccess::read))
			{
				ReleasePorts();
				return GCodeResult::error;
			}
		}

		++numPortsUsed;
		if (c != '+')
		{
			break;
		}
		++index;					// skip the "+"
	}
	return GCodeResult::ok;
}

EndStopType SwitchEndstop::GetEndstopType() const
{
	return EndStopType::inputPin;
}

// Test whether we are at or near the stop
EndStopHit SwitchEndstop::Stopped() const
{
	for (size_t i = 0; i < numPortsUsed; ++i)
	{
		if (IsTriggered(i))
		{
			return EndStopHit::atStop;
		}
	}
	return EndStopHit::noStop;
}

// This is called to prime axis endstops
bool SwitchEndstop::Prime(const Kinematics& kin, const AxisDriversConfig& axisDrivers)
{
	// Decide whether we stop just the driver, just the axis, or everything
	stopAll = ((kin.GetConnectedAxes(GetAxis()) & ~MakeBitmap<AxesBitmap>(GetAxis())) != 0);
	numPortsLeftToTrigger = (numPortsUsed != axisDrivers.numDrivers) ? 1 : numPortsUsed;
	portsLeftToTrigger = LowestNBits<PortsBitmap>(numPortsUsed);

#if SUPPORT_CAN_EXPANSION
	// For each remote switch, check that the expansion board knows about it, and make sure we have an up-to-date state
	for (size_t i = 0; i < numPortsUsed; ++i)
	{
		if (boardNumbers[i] != 0)
		{
			RemoteInputHandle h(RemoteInputHandle::typeEndstop, GetAxis(), i);
			String<StringLength100> reply;
			if (CanInterface::EnableHandle(boardNumbers[i], h, states[i], reply.GetRef()) != GCodeResult::ok)
			{
				reply.cat('\n');
				reprap.GetPlatform().Message(ErrorMessage, reply.c_str());
				return false;
			}
		}
	}
#endif

	return true;
}

// Check whether the endstop is triggered and return the action that should be performed. Don't update the state until Acknowledge is called.
// Called from the step ISR.
EndstopHitDetails SwitchEndstop::CheckTriggered(bool goingSlow)
{
	EndstopHitDetails rslt;				// initialised by default constructor
	if (portsLeftToTrigger != 0)
	{
		for (size_t i = 0; i < numPortsUsed; ++i)
		{
			if (IsBitSet(portsLeftToTrigger, i) && IsTriggered(i))
			{
				rslt.axis = GetAxis();
				if (stopAll)
				{
					rslt.SetAction(EndstopHitAction::stopAll);
					if (GetAtHighEnd())
					{
						rslt.setAxisHigh = true;
					}
					else
					{
						rslt.setAxisLow = true;
					}
				}
				else if (numPortsLeftToTrigger == 1)
				{
					rslt.SetAction(EndstopHitAction::stopAxis);
					if (GetAtHighEnd())
					{
						rslt.setAxisHigh = true;
					}
					else
					{
						rslt.setAxisLow = true;
					}
				}
				else
				{
					rslt.SetAction(EndstopHitAction::stopDriver);
					rslt.internalUse = i;			// remember which port it is, for the call to Acknowledge
					rslt.driver = reprap.GetPlatform().GetAxisDriversConfig(GetAxis()).driverNumbers[i];
				}
				break;
			}
		}
	}

	return rslt;
}

// This is called by the ISR to acknowledge that it is acting on the return from calling CheckTriggered. Called from the step ISR.
// Return true if we have finished with this endstop or probe in this move.
bool SwitchEndstop::Acknowledge(EndstopHitDetails what)
{
	switch (what.GetAction())
	{
	case EndstopHitAction::stopAll:
	case EndstopHitAction::stopAxis:
		return true;

	case EndstopHitAction::stopDriver:
		ClearBit(portsLeftToTrigger, what.internalUse);
		--numPortsLeftToTrigger;
		return false;

	default:
		return false;
	}
}

void SwitchEndstop::AppendDetails(const StringRef& str)
{
	str.catf((numPortsUsed == 1) ? "switch connected to pin" : "switches connected to pins");

	for (size_t i = 0; i < numPortsUsed; ++i)
	{
		str.cat(' ');
#if SUPPORT_CAN_EXPANSION
		if (boardNumbers[i] != CanId::MasterAddress)
		{
			RemoteInputHandle h(RemoteInputHandle::typeEndstop, GetAxis(), i);
			String<StringLength100> reply;
			if (CanInterface::GetHandlePinName(boardNumbers[i], h, states[i], reply.GetRef()) == GCodeResult::ok)
			{
				str.cat(reply.c_str());
			}
			else
			{
				reply.cat('\n');
				reprap.GetPlatform().Message(ErrorMessage, reply.c_str());
				str.catf("%u.unknown", boardNumbers[i]);
			}
		}
		else
#endif
		{
			ports[i].AppendPinName(str);
		}
	}
}

#if SUPPORT_CAN_EXPANSION

// Process a remote endstop input change that relates to this endstop
void SwitchEndstop::HandleRemoteInputChange(CanAddress src, uint8_t handleMinor, bool state)
{
	if (handleMinor < numPortsUsed && boardNumbers[handleMinor] == src)
	{
		states[handleMinor] = state;
	}
}

#endif

// End

