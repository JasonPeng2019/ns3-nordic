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


#ifndef BLE_LINK_MANAGER_H
#define BLE_LINK_MANAGER_H

// Includes
#include <ns3/callback.h>
#include <ns3/traced-callback.h>
#include <ns3/object.h>
#include <ns3/pointer.h>
#include <ns3/ptr.h>
#include <ns3/ble-link.h>
#include <ns3/nstime.h>
#include <ns3/constants.h>
#include <ns3/packet.h>
#include <ns3/simulator.h>
#include <ns3/multi-model-spectrum-channel.h>

namespace ns3 {

  // Classes
  template <typename Item> class DropTailQueue;
  class BleBBManager;
  class BleLinkController;
  class BleNetDevice;
  class QueueItem;
/** 
 * \ingroup ble
 * \brief Implementation for the Link Manager of the BLE protocol
 * This implements the following features:
 *
 */
  class BleLinkManager : public Object
  {
    public:

      // State variable
      enum State
      {
        STANDBY, ADVERTISER, SCANNER, INITIATOR, SLAVE, MASTER
      };

      enum Role
      {
        SLAVE_ROLE, MASTER_ROLE, STANDBY_ROLE, CONNECTIONLESS_ROLE 
      };

      enum StateType
      {
        CONNECTIONLESS, CONNECTED
      };

      BleLinkManager ();
      ~BleLinkManager ();

      void DoDispose ();

      static TypeId GetTypeId (void);

      void SetState(State state);
      State GetState();

      bool IsConnected();

      void NextState();

      // Setup a Link to a specific receiver with the role as 
      // expected role at the end of the setup process.

      // nbConnectionInterval == 0 means random connectioninterval
      void SetupLink(Role myRole, 
          Ptr<BleLinkManager> otherLinkManager, bool scheduled, 
          uint32_t nbTxWindowOffset, uint32_t nbConnectionInterval);

      void SetupLink(
          std::vector<Ptr<BleLinkManager>> otherLinkManagers, 
          bool scheduled, uint32_t nbTxWindowOffset, 
          uint32_t nbConnectionInterval, bool collAvoid);

      Ptr<BleLink> GetAssociatedLink();
      void SetAssociatedLink(Ptr<BleLink> link);

      Ptr<BleBBManager> GetBBManager();
      void SetBBManager (Ptr<BleBBManager> bbm);

      // Getters and setters for scheduling parameters
      void SetConnInterval (Time connInterval);
      void SetConnSlaveLatency (uint16_t connSlaveLatency);
      void SetConnSupervisionTimeout (Time connSupervisionTimeout);
      void SetConnEventCounter (uint16_t connEventCounter);
      void SetTransmitWindowOffset (Time transmitWindowOffset);
      void SetTransmitWindowSize (Time transmitWindowSize);
      Time GetConnInterval (void);
      uint16_t GetConnSlaveLatency (void);
      Time GetConnSupervisionTimeout (void);
      uint16_t GetConnEventCounter (void);
      Time GetTransmitWindowOffset (void);
      Time GetTransmitWindowSize (void);

      /*
       * Returns the last Time that the connection was established
       * this equals the end of the last CONNECT_REQ PDU that was received.
       */
      Time GetLastTimeConnectionEstablished (void);
      void SetLastTimeConnectionEstablished (Time lasttime);

      /*
       * Returns the start of the previous transmit window.
       */
      Time GetLastTransmitWindowTime (void);
      void SetLastTransmitWindowTime (Time lasttime);



      // Returns the next time that the device can send
      // a packet over the associated link
      // Also, this time is set to be the beginning of
      // the last transmit window and will be used for the
      // calculation of the next transmit window.
      Time GetNextTransmitWindowTime (void);

      /*
       * Returns true if 'thisTime' is inside the transmitWindow
       * that was last calculated.
       */
      bool IsInsideLastTransmitWindow (Time thisTime);

      /*
       * Put packet in the queue / buffer, so it can be transmitted
       */
      Ptr<DropTailQueue<QueueItem>> GetQueue (void);

      void SetCurrentPacket (Ptr<Packet> packet);
      Ptr<Packet> GetCurrentPacket (void);

      void StartTransmitWindow (void);
      void EndTransmitWindow (void);
      void PrepareNextTransmitWindow (void);

      void HandleTXDone (void);
      void SendNextPacket (void);

      void SetPeerHasMoreData (bool md);
      bool GetPeerHasMoreData (void);

      /*
       * If true: SLAVE will answer so the connection is not lost
       * if there is no data to send.
       */
      void SetKeepAliveActive (bool keepAliveActive);

      /*
       * Returns true if the device needs to send at least
       * one packet in this TX window, even though it has
       * no data in the queue
       */
      bool NeedToSendAtLeastOne();

     // void SetPreviousNeedsAck(bool needsAck);
      void SetMyLastMD(bool md); // The last md bit that I have send to peer.
      bool GetMyLastMD ();


      // Returns true if TX new data
      bool ManageSequenceNumberTX (void);
      // Returns true if RX new data
      bool ManageSequenceNumberRX (void);

      void SetSN (bool sn);
      void SetNESN (bool nesn);

      // Hop increment is used for channel selection algorithm, 
      // should be a random number
      void SetHopIncrement (uint8_t hopIncrement);

      void ManageChannelSelection ();

      bool IsUsedChannel (uint8_t channelIndex);
      void SetUsedChannels (std::vector<uint8_t> usedChannels);

      uint8_t GetCurrentChannelIndex ();

      void SetAdvSleepCounter (uint16_t cntr);
      void SetMaxAdvSleep (uint16_t max_counter);
      void SetAdvCollisionAvoidance (bool collAvoid);

    private:

      // This is false as long as no transmit window has past
      // sinds last connection establishment. This value is
      // set to false by the SetLastTimeConnectionEstablished()
      // function, set to true by SetLastTransmitWindowTime ()
      bool m_firstTransmitWindowDone;

      EventId m_nextWindow;
      EventId m_endOfCurrentWindow;

      State currentState;
      Role expectedRole;
      Ptr<BleLink> m_associatedLink; 
      //<! there is a link manager for each link, 
      // this is the link associated to this link manager

      Time m_lastTimeConnectionEstablished;
      Time m_lastTransmitWindowTime;

      // Scheduling parameters 
      Time m_connInterval;
      uint16_t m_connSlaveLatency;
      // Max time between two received 
      // Data packet PDUs before connection is considered lost
      Time m_connSupervisionTimeout; 
      uint16_t m_connEventCounter;
      Time m_transmitWindowOffset;
      Time m_transmitWindowSize;

      // Packet buffer
      Ptr<DropTailQueue<QueueItem>> m_queue;

      Ptr<BleBBManager> m_bbManager;
      Ptr<Packet> m_currentPacket;
      bool m_currentIsDummy;

      bool m_nextExpectedSequenceNumber;
      bool m_sequenceNumber;
      bool m_peerHasMoreData;
      bool m_onePacketSend; 
      //m_onePacketSend is true if there was one packet send inside this TX window
      bool m_keepAliveActive;

      bool m_lastMD;

      uint16_t m_advSleepCounter; 
      // Keeps track of the number of advertising events in which 
      // this link manager cannot send a packet. 
      // Only valid if broadcastCollisionAvoidance is enabled.

      uint16_t m_advSleepMax;
      bool m_broadcastCollisionAvoidance;

      uint8_t m_lastUnmappedChannelIndex;
      uint8_t m_unmappedChannelIndex;
      uint8_t m_hopIncrement;
      uint8_t m_dataChannelIndex;
      std::vector<uint8_t> m_usedChannels;
  };
}
#endif /* BLE_LINK_MANAGER_H */

