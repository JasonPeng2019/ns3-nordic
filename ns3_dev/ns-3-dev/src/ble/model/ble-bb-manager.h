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


#ifndef BLE_BB_MANAGER_H
#define BLE_BB_MANAGER_H

// Includes
#include <ns3/callback.h>
#include <ns3/traced-callback.h>
#include <ns3/object.h>
#include <ns3/ble-phy.h>
#include <ns3/ble-link-manager.h>

#include <ns3/generic-phy.h>

#include <ns3/ptr.h>
#include <ns3/pointer.h>
#include <ns3/packet.h>
#include <ns3/simulator.h>

#include <ns3/constants.h>

namespace ns3 {

  // Classes

  template <typename Item> class DropTailQueue;
  class QueueItem;
  class BleLinkController;
  class BleLink;
  class BleNetDevice;

/** 
 * \ingroup ble
 * \brief Implementation for the broadbandmanager of the BLE protocol
 * This implements the following features:
 *
 */
  class BleBBManager : public Object
  {
    public:

      BleBBManager ();
      BleBBManager (Ptr<BleNetDevice> bleNetDevice);
      ~BleBBManager ();
      void DoDispose (void);

      static TypeId GetTypeId (void);

      BlePhy::State GetPhyState ();

      Ptr<BlePhy> GetPhy ();
      Ptr<BleLinkController> GetLinkController ();
      Ptr<BleNetDevice> GetNetDevice ();
      void SetNetDevice (Ptr<BleNetDevice> netDevice);
      void SetPhy (Ptr<BlePhy> phy);
      Ptr<DropTailQueue<QueueItem>> GetQueue (void);

      Ptr<Packet> GetCurrentPacket();
      void SetCurrentPacket(Ptr<Packet> packet);

      // Add a link to the list of links that can be associated
      // to this device
      void AddLinkManager(Ptr<BleLinkManager> linkManager);
      Ptr<BleLink> CreateLink(Ptr<BleBBManager> otherBBManager, 
          BleLinkManager::Role myRole);
      Ptr<BleLink> CreateLinkScheduled(Ptr<BleBBManager> otherBBManager, 
          BleLinkManager::Role myRole, bool scheduled, 
          uint32_t nbTxWindowOffset, uint32_t nbConnectionInterval);
      
      // Create a link with multiple nodes. this device will be the master
      Ptr<BleLink> CreateLinkScheduledMultipleNodes(
          std::list<Ptr<BleBBManager>> otherBBManagers, bool scheduled, 
          uint32_t nbTxWindowOffset, uint32_t nbConnectionInterval, 
          bool collAvoid);

      // Check if a specific link exists
      bool LinkExists (Ptr<BleLink> link);
      bool LinkManagerExists (Ptr<BleLinkManager> linkManager);

      // Check if a link to a device with a specific address exists
      bool LinkExists (Mac16Address address);
      // Get link to a specific address.
      Ptr<BleLink> GetLink (Mac16Address address);
      Ptr<BleLinkManager> GetLinkManager (Mac16Address address);

      uint32_t CountLinks ();

      void TryAgain();

      /*
       * Checks if there is a new item in the netdevice queue qnd
       * forwards this item to the right LinkManager's queue.
       */
      void HandlePacket ();

      /*
       * The link manager that has control over the phy device at the moment
       * this is necessary so we could reply using the right parameters / 
       * window etc.
       */
      void SetActiveLinkManager(Ptr<BleLinkManager> lm);
      Ptr<BleLinkManager> GetActiveLinkManager();

    private:
      Ptr<BleNetDevice> m_netDevice;
      std::list<Ptr<BleLinkManager>> m_linkManagers; 

      // The LinkManager that has control over the device
      // at this moment
      Ptr<BleLinkManager> m_activeLinkManager;
 };

}

#endif /* BLE_BB_MANAGER_H */

