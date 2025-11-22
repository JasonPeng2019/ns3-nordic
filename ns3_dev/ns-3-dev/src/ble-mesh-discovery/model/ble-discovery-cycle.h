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

#ifndef BLE_DISCOVERY_CYCLE_H
#define BLE_DISCOVERY_CYCLE_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/callback.h"
#include <vector>

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief Implements the 4-slot discovery cycle for BLE mesh discovery protocol
 *
 * The discovery cycle manages timing for:
 * - Slot 0: Own discovery message transmission
 * - Slots 1-3: Forwarding received discovery messages
 *
 * This class handles cycle timing, slot scheduling, and synchronization.
 */
class BleDiscoveryCycle : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  BleDiscoveryCycle ();

  /**
   * \brief Destructor
   */
  virtual ~BleDiscoveryCycle ();

  /**
   * \brief Start the discovery cycle
   */
  void Start ();

  /**
   * \brief Stop the discovery cycle
   */
  void Stop ();

  /**
   * \brief Check if the cycle is running
   * \return true if cycle is active
   */
  bool IsRunning () const;

  /**
   * \brief Set the duration of a single slot
   * \param duration Time duration for each slot
   */
  void SetSlotDuration (Time duration);

  /**
   * \brief Get the slot duration
   * \return Time duration of each slot
   */
  Time GetSlotDuration () const;

  /**
   * \brief Get the total cycle duration (4 slots)
   * \return Total cycle time
   */
  Time GetCycleDuration () const;

  /**
   * \brief Get the current slot number (0-3)
   * \return Current slot number
   */
  uint8_t GetCurrentSlot () const;

  /**
   * \brief Set callback for slot 0 (own message transmission)
   * \param callback Function to call at slot 0
   */
  void SetSlot0Callback (Callback<void> callback);

  /**
   * \brief Set callback for forwarding slots (1-3)
   * \param slotNumber Slot number (1, 2, or 3)
   * \param callback Function to call at the specified slot
   */
  void SetForwardingSlotCallback (uint8_t slotNumber, Callback<void> callback);

  /**
   * \brief Set callback for cycle completion
   * \param callback Function to call when cycle completes
   */
  void SetCycleCompleteCallback (Callback<void> callback);

private:
  /**
   * \brief Execute slot 0 (own message transmission)
   */
  void ExecuteSlot0 ();

  /**
   * \brief Execute forwarding slot (1-3)
   * \param slotNumber The slot number to execute
   */
  void ExecuteForwardingSlot (uint8_t slotNumber);

  /**
   * \brief Schedule the next cycle
   */
  void ScheduleNextCycle ();

  bool m_running;                              //!< Whether cycle is active
  Time m_slotDuration;                         //!< Duration of each slot
  uint8_t m_currentSlot;                       //!< Current slot number (0-3)

  EventId m_slot0Event;                        //!< Event for slot 0
  EventId m_slot1Event;                        //!< Event for slot 1
  EventId m_slot2Event;                        //!< Event for slot 2
  EventId m_slot3Event;                        //!< Event for slot 3
  EventId m_cycleEvent;                        //!< Event for next cycle

  Callback<void> m_slot0Callback;              //!< Callback for slot 0
  Callback<void> m_slot1Callback;              //!< Callback for slot 1
  Callback<void> m_slot2Callback;              //!< Callback for slot 2
  Callback<void> m_slot3Callback;              //!< Callback for slot 3
  Callback<void> m_cycleCompleteCallback;      //!< Callback for cycle completion
};

} // namespace ns3

#endif /* BLE_DISCOVERY_CYCLE_H */
