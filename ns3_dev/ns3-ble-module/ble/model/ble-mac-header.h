/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 KU Leuven
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Author: kwong yin <kwong-sang.yin@boeing.com> 
 *          Stijn Geysen <stijn.geysen@student.kuleuven.be>
 *   Simplification and adaptation 
 *   from the LoRa ns-3 module lora-mac-header,
 *   written by Brecht Reynders.
 *   This module can be found here:
 *   github.com/networkedsystems/lora-ns3/blob/master/model/lora-mac-header.h
 */

#ifndef BLE_MAC_HEADER_H
#define BLE_MAC_HEADER_H

#include <ns3/header.h>
#include <ns3/mac16-address.h>

namespace ns3 {

/*
 * \ingroup ble
 * Represent the Mac Header 
 * */
class BleMacHeader : public Header
{

public:

  BleMacHeader (void);


  ~BleMacHeader (void);


  Mac16Address GetDestAddr (void) const;
  Mac16Address GetSrcAddr (void) const;

  uint16_t GetProtocol (void) const;

  bool GetNESN (void) const; // Get Next Expected Sequence Number bit
  bool GetSN (void) const; // Get Sequence Number bit
  bool GetMD (void) const; // Get More Data bit
  uint8_t GetLLID (void) const;
  uint8_t GetLength (void) const;

  void SetSrcAddr ( Mac16Address addr);
  void SetDestAddr ( Mac16Address addr);

  void SetProtocol (uint16_t protocol);

  void SetNESN (bool nesn);
  void SetSN (bool sn);
  void SetMD (bool md);
  void SetLLID (uint8_t llid);
  void SetLength (uint8_t length);

  std::string GetName (void) const;
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  void Print (std::ostream &os) const;
  uint32_t GetSerializedSize (void) const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);

private:
  /* Addressing fields */
  Mac16Address m_src_addr;        // 0 or 8 Octet
  Mac16Address m_dest_addr;        // 0 or 8 Octet
  
  uint16_t m_protocol;

  bool m_nesn;
  bool m_sn;
  bool m_md;
  uint8_t m_llid; // this is only 2 bits
  uint8_t m_length; // 5 bits long
  uint8_t m_rfu; //6 bits reserved for future use
}; //BleMacHeader

}; // namespace ns-3

#endif /* BLE_MAC_HEADER_H */
