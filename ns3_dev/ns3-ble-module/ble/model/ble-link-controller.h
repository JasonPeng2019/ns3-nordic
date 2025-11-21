/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 KULeuven 
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
 * Author: Stijn Geysen <stijn.geysen@student.kuleuven.be>
 */


#ifndef BLE_LINK_CONTROLLER_H
#define BLE_LINK_CONTROLLER_H

// Includes
#include <ns3/callback.h>
#include <ns3/traced-callback.h>
#include <ns3/object.h>
#include <ns3/packet.h>
#include <ns3/pointer.h>

#include <ns3/generic-phy.h>
#include <ns3/callback.h>

#include <ns3/constants.h>
#include <ns3/spectrum-channel.h>

namespace ns3 {

  // Classes

  class BleNetDevice;
  class BleBBManager;
  class BleLinkManager;
  class BlePhy;

/** 
 * \ingroup ble
 * \brief Implementation for the Link Controller of the BLE protocol
 *
 */
  class BleLinkController : public Object
  {
    public:

      BleLinkController ();
      ~BleLinkController ();
      void DoDispose (void);

      static TypeId GetTypeId (void);

      // Setters and getters
      void SetNetDevice(Ptr<BleNetDevice> netDevice);
      Ptr<BleNetDevice> GetNetDevice();
      void SetPhy(Ptr<BlePhy> phy);
      Ptr<BlePhy> GetPhy();
      void SetBBManager(Ptr<BleBBManager> bbManager);
      Ptr<BleBBManager> GetBBManager();

      Ptr<Packet> GetCurrentPacket();
      void SetCurrentPacket(Ptr<Packet> packet);
      uint8_t GetRetransmissionCount ();
      void SetRetransmissionCount(uint8_t count);

      Time GetStartTimePacket();
      void SetStartTimePacket(Time time);
      
      void StartTransmissionNoArgs(); 

      void StartPacketTransmission(Ptr<BleLinkManager> lm);
      void PrepareForReception (Ptr<BleLinkManager> lm);

      // Callback functions
      void SetGenericPhyTxStartCallback (GenericPhyTxStartCallback c);

      void CheckReceivedAckPacket (Ptr<Packet> packet, bool receptionError);


      void SetCheckedAckCallback (Callback<void, Ptr<Packet> > callback);
      void SetCheckedAckErrorCallback (Callback<void, Ptr<Packet> > callback);

      void SetAllChannels (std::vector<Ptr<SpectrumChannel>> allChannels);

      Ptr<SpectrumChannel> GetChannelBasedOnChannelIndex(uint8_t channelIndex);
    private:
      Ptr<BleNetDevice> m_netDevice; // Associated netdevice

      Ptr<Packet> m_currentPkt; //!< packet that is current being transmitted

      uint8_t retransmissionCount; //!< number of retransmissions of current packet
      Time startTimePacket; //!< time that device tried to send a 
                            //   packet for the first time
      Time lastSend; //!< time at which was last transmission 
      Callback<void, Ptr<Packet> > m_ackChecked;
      Callback<void, Ptr<Packet> > m_ackCheckedError;
      // Traceback functions:
      TracedCallback<Ptr<const Packet> > m_macTxTrace;

      // List of callbacks to fire if the link changes state (up or down).
      GenericPhyTxStartCallback m_phyMacTxStartCallback;

      // Functions:
      
      bool StartTransmission (Ptr<Packet> packet, bool ackPacket);
      
      std::vector<Ptr<SpectrumChannel>> m_allChannels;
  };

}

#endif /* BLE_LINK_CONTROLLER_H */

