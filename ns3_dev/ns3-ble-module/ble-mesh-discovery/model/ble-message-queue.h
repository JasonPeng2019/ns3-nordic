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

#ifndef BLE_MESSAGE_QUEUE_WRAPPER_H
#define BLE_MESSAGE_QUEUE_WRAPPER_H

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ble-discovery-header-wrapper.h"

extern "C" {
#include "ns3/ble_message_queue.h"
}

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief C++ wrapper for BLE message queue (wraps pure C implementation)
 *
 * This class provides an NS-3 interface to the portable C message queue implementation.
 * The C core handles all logic and can be compiled for embedded systems.
 */
class BleMessageQueue : public Object
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
  BleMessageQueue ();

  /**
   * \brief Destructor
   */
  virtual ~BleMessageQueue ();

  /**
   * \brief Add a message to the queue
   * \param packet The packet to queue (will be copied)
   * \param header The parsed discovery header
   * \param nodeId This node's ID (for loop detection)
   * \return true if message was added, false if rejected (duplicate or loop)
   */
  bool Enqueue (Ptr<Packet> packet, const BleDiscoveryHeaderWrapper& header, uint32_t nodeId);

  /**
   * \brief Get the next message to forward
   * \param header Output parameter for the message header
   * \return The highest priority message packet, or nullptr if queue empty
   */
  Ptr<Packet> Dequeue (BleDiscoveryHeaderWrapper& header);

  /**
   * \brief Get the next message header without removing from queue
   * \param header Output parameter for the message header
   * \return true if queue has messages, false if empty
   */
  bool Peek (BleDiscoveryHeaderWrapper& header) const;

  /**
   * \brief Check if queue is empty
   * \return true if no messages in queue
   */
  bool IsEmpty () const;

  /**
   * \brief Get number of messages in queue
   * \return Queue size
   */
  uint32_t GetSize () const;

  /**
   * \brief Clear all messages from queue
   */
  void Clear ();

  /**
   * \brief Clean old entries from the seen messages cache
   * \param maxAge Maximum age for cached entries
   */
  void CleanOldEntries (Time maxAge);

  /**
   * \brief Get statistics on queue performance
   * \param totalEnqueued Total messages enqueued
   * \param totalDequeued Total messages dequeued
   * \param totalDuplicates Total duplicates rejected
   * \param totalLoops Total loops detected
   * \param totalOverflows Total messages rejected due to overflow
   */
  void GetStatistics (uint32_t& totalEnqueued, uint32_t& totalDequeued,
                      uint32_t& totalDuplicates, uint32_t& totalLoops,
                      uint32_t& totalOverflows) const;

private:
  ble_message_queue_t m_queue;             //!< C core queue structure
};

} // namespace ns3

#endif /* BLE_MESSAGE_QUEUE_WRAPPER_H */
