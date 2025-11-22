/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Your Institution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ble-discovery-cycle.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryCycle");
NS_OBJECT_ENSURE_REGISTERED (BleDiscoveryCycle);

TypeId
BleDiscoveryCycle::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleDiscoveryCycle")
    .SetParent<Object> ()
    .SetGroupName ("BLE")
    .AddConstructor<BleDiscoveryCycle> ()
    .AddAttribute ("SlotDuration",
                   "Duration of each discovery slot",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&BleDiscoveryCycle::m_slotDuration),
                   MakeTimeChecker ())
  ;
  return tid;
}

BleDiscoveryCycle::BleDiscoveryCycle ()
  : m_running (false),
    m_slotDuration (MilliSeconds (100)),
    m_currentSlot (0)
{
  NS_LOG_FUNCTION (this);
}

BleDiscoveryCycle::~BleDiscoveryCycle ()
{
  NS_LOG_FUNCTION (this);
  Stop ();
}

void
BleDiscoveryCycle::Start ()
{
  NS_LOG_FUNCTION (this);

  if (m_running)
    {
      NS_LOG_WARN ("Discovery cycle already running");
      return;
    }

  m_running = true;
  m_currentSlot = 0;

  NS_LOG_INFO ("Starting discovery cycle with slot duration: " << m_slotDuration.GetMilliSeconds () << " ms");

  // Schedule slot 0 immediately
  m_slot0Event = Simulator::Schedule (Seconds (0), &BleDiscoveryCycle::ExecuteSlot0, this);

  // Schedule slots 1, 2, 3
  m_slot1Event = Simulator::Schedule (m_slotDuration, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 1);
  m_slot2Event = Simulator::Schedule (m_slotDuration * 2, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 2);
  m_slot3Event = Simulator::Schedule (m_slotDuration * 3, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 3);

  // Schedule next cycle
  m_cycleEvent = Simulator::Schedule (m_slotDuration * 4, &BleDiscoveryCycle::ScheduleNextCycle, this);
}

void
BleDiscoveryCycle::Stop ()
{
  NS_LOG_FUNCTION (this);

  if (!m_running)
    {
      return;
    }

  m_running = false;

  // Cancel all scheduled events
  Simulator::Cancel (m_slot0Event);
  Simulator::Cancel (m_slot1Event);
  Simulator::Cancel (m_slot2Event);
  Simulator::Cancel (m_slot3Event);
  Simulator::Cancel (m_cycleEvent);

  NS_LOG_INFO ("Discovery cycle stopped");
}

bool
BleDiscoveryCycle::IsRunning () const
{
  return m_running;
}

void
BleDiscoveryCycle::SetSlotDuration (Time duration)
{
  NS_LOG_FUNCTION (this << duration);

  if (m_running)
    {
      NS_LOG_WARN ("Cannot change slot duration while cycle is running");
      return;
    }

  m_slotDuration = duration;
}

Time
BleDiscoveryCycle::GetSlotDuration () const
{
  return m_slotDuration;
}

Time
BleDiscoveryCycle::GetCycleDuration () const
{
  return m_slotDuration * 4;
}

uint8_t
BleDiscoveryCycle::GetCurrentSlot () const
{
  return m_currentSlot;
}

void
BleDiscoveryCycle::SetSlot0Callback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_slot0Callback = callback;
}

void
BleDiscoveryCycle::SetForwardingSlotCallback (uint8_t slotNumber, Callback<void> callback)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (slotNumber));

  if (slotNumber < 1 || slotNumber > 3)
    {
      NS_LOG_ERROR ("Invalid forwarding slot number: " << static_cast<uint32_t> (slotNumber));
      return;
    }

  switch (slotNumber)
    {
    case 1:
      m_slot1Callback = callback;
      break;
    case 2:
      m_slot2Callback = callback;
      break;
    case 3:
      m_slot3Callback = callback;
      break;
    }
}

void
BleDiscoveryCycle::SetCycleCompleteCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_cycleCompleteCallback = callback;
}

void
BleDiscoveryCycle::ExecuteSlot0 ()
{
  NS_LOG_FUNCTION (this);
  m_currentSlot = 0;

  NS_LOG_DEBUG ("Executing Slot 0 - Own message transmission");

  if (!m_slot0Callback.IsNull ())
    {
      m_slot0Callback ();
    }
}

void
BleDiscoveryCycle::ExecuteForwardingSlot (uint8_t slotNumber)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (slotNumber));
  m_currentSlot = slotNumber;

  NS_LOG_DEBUG ("Executing Slot " << static_cast<uint32_t> (slotNumber) << " - Forwarding");

  Callback<void> callback;

  switch (slotNumber)
    {
    case 1:
      callback = m_slot1Callback;
      break;
    case 2:
      callback = m_slot2Callback;
      break;
    case 3:
      callback = m_slot3Callback;
      break;
    default:
      NS_LOG_ERROR ("Invalid slot number");
      return;
    }

  if (!callback.IsNull ())
    {
      callback ();
    }
}

void
BleDiscoveryCycle::ScheduleNextCycle ()
{
  NS_LOG_FUNCTION (this);

  if (!m_running)
    {
      return;
    }

  // Call cycle complete callback
  if (!m_cycleCompleteCallback.IsNull ())
    {
      m_cycleCompleteCallback ();
    }

  NS_LOG_DEBUG ("Cycle complete, scheduling next cycle");

  // Restart the cycle
  m_currentSlot = 0;

  m_slot0Event = Simulator::Schedule (Seconds (0), &BleDiscoveryCycle::ExecuteSlot0, this);
  m_slot1Event = Simulator::Schedule (m_slotDuration, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 1);
  m_slot2Event = Simulator::Schedule (m_slotDuration * 2, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 2);
  m_slot3Event = Simulator::Schedule (m_slotDuration * 3, &BleDiscoveryCycle::ExecuteForwardingSlot, this, 3);
  m_cycleEvent = Simulator::Schedule (m_slotDuration * 4, &BleDiscoveryCycle::ScheduleNextCycle, this);
}

} // namespace ns3
