/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper for Pure C Protocol Implementation
 * This wraps the C protocol core to work with NS-3's C++ framework
 */

#ifndef BLE_DISCOVERY_HEADER_WRAPPER_H
#define BLE_DISCOVERY_HEADER_WRAPPER_H

#include "ns3/header.h"
#include "ns3/vector.h"
#include "ns3/ble_discovery_packet.h"
#include <vector>

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief NS-3 C++ Wrapper for BLE Discovery Protocol (Pure C Implementation)
 *
 * This class provides a thin C++ wrapper around the pure C protocol implementation,
 * allowing it to integrate with NS-3 while keeping the core logic portable.
 */
class BleDiscoveryHeaderWrapper : public Header
{
public:
  /**
   * \brief Constructor
   */
  BleDiscoveryHeaderWrapper ();

  /**
   * \brief Destructor
   */
  virtual ~BleDiscoveryHeaderWrapper ();

  /**
   * \brief Get the type ID
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the type ID for this instance
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * \brief Print the header to an output stream
   * \param os output stream
   */
  virtual void Print (std::ostream &os) const;

  /**
   * \brief Get the serialized size of the header
   * \return the serialized size
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the header to a buffer
   * \param start the buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the header from a buffer
   * \param start the buffer iterator
   * \return the number of bytes read
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

  // ===== C++ Convenience Methods (delegates to C core) =====

  /**
   * \brief Check if this is an election announcement
   * \return true if election message
   */
  bool IsElectionMessage (void) const;

  /**
   * \brief Set sender ID
   * \param id the sender ID
   */
  void SetSenderId (uint32_t id);

  /**
   * \brief Get sender ID
   * \return the sender ID
   */
  uint32_t GetSenderId (void) const;

  /**
   * \brief Set TTL
   * \param ttl the TTL value
   */
  void SetTtl (uint8_t ttl);

  /**
   * \brief Get TTL
   * \return the TTL value
   */
  uint8_t GetTtl (void) const;

  /**
   * \brief Decrement TTL
   * \return true if TTL > 0 after decrement
   */
  bool DecrementTtl (void);

  /**
   * \brief Add node to path
   * \param nodeId the node ID
   * \return true if added successfully
   */
  bool AddToPath (uint32_t nodeId);

  /**
   * \brief Check if node is in path
   * \param nodeId the node ID
   * \return true if in path
   */
  bool IsInPath (uint32_t nodeId) const;

  /**
   * \brief Get path as C++ vector
   * \return vector of node IDs
   */
  std::vector<uint32_t> GetPath (void) const;

  /**
   * \brief Set GPS location
   * \param position NS-3 Vector
   */
  void SetGpsLocation (Vector position);

  /**
   * \brief Get GPS location
   * \return NS-3 Vector
   */
  Vector GetGpsLocation (void) const;

  /**
   * \brief Set GPS available flag
   * \param available GPS availability
   */
  void SetGpsAvailable (bool available);

  /**
   * \brief Check if GPS is available
   * \return true if GPS available
   */
  bool IsGpsAvailable (void) const;

  // ===== Election-specific methods =====

  /**
   * \brief Convert to election message
   */
  void SetAsElectionMessage (void);

  /**
   * \brief Set class ID
   * \param classId the class ID
   */
  void SetClassId (uint16_t classId);

  /**
   * \brief Get class ID
   * \return the class ID
   */
  uint16_t GetClassId (void) const;

  /**
   * \brief Set PDSF
   * \param pdsf the PDSF value
   */
  void SetPdsf (uint32_t pdsf);

  /**
   * \brief Get PDSF
   * \return the PDSF value
   */
  uint32_t GetPdsf (void) const;

  /**
   * \brief Set score
   * \param score the score
   */
  void SetScore (double score);

  /**
   * \brief Get score
   * \return the score
   */
  double GetScore (void) const;

  /**
   * \brief Set hash
   * \param hash the hash value
   */
  void SetHash (uint32_t hash);

  /**
   * \brief Get hash
   * \return the hash value
   */
  uint32_t GetHash (void) const;

  // ===== Direct access to C structures (for advanced use) =====

  /**
   * \brief Get reference to underlying C packet structure
   * \return reference to discovery packet
   */
  const ble_discovery_packet_t& GetCPacket (void) const { return m_packet; }

  /**
   * \brief Get reference to underlying C election packet structure
   * \return reference to election packet
   */
  const ble_election_packet_t& GetCElectionPacket (void) const { return m_election; }

private:
  bool m_isElection;                  //!< Track if this is election message
  ble_discovery_packet_t m_packet;    //!< C discovery packet structure
  ble_election_packet_t m_election;   //!< C election packet structure
};

} // namespace ns3

#endif /* BLE_DISCOVERY_HEADER_WRAPPER_H */
