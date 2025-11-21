/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 CTTC
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 * Author: Stijn Geysen <stijn.geysen@student.kuleuven.be>
 *          Based on the lora ns-3 module written by Brecht Reynders.
 *          This module can be found here:
 *https://github.com/networkedsystems/lora-ns3/blob/master/model/lora-mac-header.h
 */

#ifndef BLE_PHY_H
#define BLE_PHY_H


#include <ns3/object.h>
#include <ns3/nstime.h>
#include <ns3/spectrum-channel.h>
#include <ns3/spectrum-phy.h>
#include "ble-error-model.h"
#include "ble-spectrum-signal-parameters.h"
#include <ns3/event-id.h>
#include <ns3/random-variable-stream.h>
namespace ns3 {

const int NB_BANDS = 40;
const double BANDWIDTH = 2e6;
const double TX_PREP_TIME = 50; 
// time needed to start up transmitter in microseconds
const double RX_PREP_TIME = 50; 
// time needed to start up receiver in microseconds

class SpectrumChannel;
class MobilityModel;
class AntennaModel;
class SpectrumValue;
class SpectrumModel;
class NetDevice;
struct SpectrumSignalParameters;

class BleBBManager;

/**
 * \ingroup spectrum
 *
 * Abstract base class for Spectrum-aware PHY layers
 *
 */
class BlePhy : public SpectrumPhy
{

public:
  BlePhy ();
  ~BlePhy ();

  /**
   * State of the transceiver
   */
  enum State
  {
    IDLE,
    TX,
    TX_BUSY,
    RX,
    RX_BUSY 
  };

  static TypeId GetTypeId (void);

  /**
   * set the associated NetDevice instance
   *
   * @param d the NetDevice instance
   */
  void SetDevice (Ptr<NetDevice> d);

  /**
   * get the associated NetDevice instance
   *
   * @return a Ptr to the associated NetDevice instance
   */
  Ptr<NetDevice> GetDevice () const;
  Ptr<BleBBManager> GetBBManager ();

  /**
   * Set the mobility model associated with this device.
   *
   * @param m the mobility model
   */
  void SetMobility (Ptr<MobilityModel> m);

  /**
   * get the associated MobilityModel instance
   *
   * @return a Ptr to the associated MobilityModel instance
   */
  Ptr<MobilityModel> GetMobility () const;

  /**
   * Set the channel attached to this device.
   *
   * @param c the channel
   */
  void SetChannel (Ptr<SpectrumChannel> c);
  Ptr<SpectrumChannel> GetChannel();

  /**
   *
   * @return returns the SpectrumModel that this SpectrumPhy expects to be used
   * for all SpectrumValues that are passed to StartRx. If 0 is
   * returned, it means that any model will be accepted.
   */
  Ptr<const SpectrumModel> GetRxSpectrumModel () const;
  void SetRxSpectrumModel (Ptr<const SpectrumModel> model);

  /**
   * Set the Tx power spectrum by setting channel and power
   *
   * @param channeloffset number of channel used
   * @param power total radiated power
   */
   void InitTxPowerSpectralDensity (uint8_t channeloffset, double power);
   void SetTxPowerSpectralDensity (uint8_t channeloffset, double power);

  /**
   * get the AntennaModel used by the NetDevice for reception
   *
   * @return a Ptr to the AntennaModel used by the NetDevice for reception
   */
  Ptr<AntennaModel> GetRxAntenna () const;
  void SetRxAntenna (Ptr<AntennaModel> a);

 /**
   * Notify the SpectrumPhy instance of an incoming signal
   *
   * @param params the parameters of the signals being received
   */
  void StartRx (Ptr<SpectrumSignalParameters> params);

  /**
   * Verify if packet is correctly received. 
   *
   * @param params the parameters of the signals being received
   */
  void EndRx (Ptr<BleSpectrumSignalParameters> params);
  void EndNoise (Ptr<SpectrumValue> sv);
  /**
   *
   */
  void SetTransmissionEndCallback(Callback<void,Ptr<const Packet> > callback);
 
  /**
   *
   */
  void SetReceptionStartCallback(Callback<void> callback);
  
  /**
   *
   */
  void SetReceptionEndCallback(Callback<void,Ptr<Packet>, bool> callback);
  
  /**
   *
   */
  void SetReceptionErrorCallback(Callback<void> callback);

  /**
   *
   */
  bool StartTx (Ptr<Packet> packet);
  void EndTx (Ptr<Packet> packet);
  /**
   *
   */
  void SetReceiverMode (bool receiver);

  void SetChannelIndex(uint8_t channelIndex);
  void SetPower (double power);
  void SetBandwidth (uint32_t bandwidth);


  BlePhy::State GetState ();
  void ChangeState (BlePhy::State state);

  // TX states
  bool PrepareTX (Ptr<Packet> packet); // Startup transmitter
  
  // RX states
  bool PrepareRX (); // Startup receiver and look for preamble

  bool SetIdle (); // Return to the IDLE state and turn transceiver off

private:
 Ptr<NetDevice> m_netDevice; //upper layer
 Ptr<MobilityModel> m_mobility; //position
 Ptr<SpectrumChannel> m_channel; //channel to transmit on
 Ptr<SpectrumValue> m_txPsd; //Current transmit psd
 Ptr<AntennaModel> m_antenna; //antenna to be used
 bool m_receiver; // whether or not this physical layer 
                  // is a sender or receiver 
                  // (only monodirectional traffic)
 double m_k; //boltzman
 double m_temperature; //noise temperature
 double m_bandWidth; //bandwith
 double m_bitrate; //bitrate
 double m_power; //power of transmission
 uint8_t m_channelIndex; //channel to transmit on
 double m_bitErrors[40]; //biterrors collected 
 std::vector <Ptr<BleSpectrumSignalParameters> > m_params; 
            //all transmissions that are happening at the moment
 EventId m_events[40]; //current receiving events for sending
 double m_lastCheck; //last time check
 double m_equivalentNoiseTemperature; //noise temperature
 Ptr<SpectrumValue> m_receivingPower; //all the power at the receiving antenna
 Ptr<BleErrorModel> m_errorModel; // error model for this device
 Ptr<UniformRandomVariable> m_random; //determines whether received package 
                                      //is lost are not
 Ptr<UniformRandomVariable> m_channelSelector; //selects the channel
 //callbackfunctions
 Callback<void, Ptr<const Packet> > m_transmissionEnd; 
 Callback<void> m_ReceptionStart;
 Callback<void> m_ReceptionError;
 Callback<void, Ptr<Packet>, bool > m_ReceptionEnd;

 BlePhy::State m_currentState;



  /**
  * Create spectrumValues for BLE signal
  *
  * @param channeloffset tells system where signal is situated in spectrum
  * @param power tells system how strong the signal is (in W)
  *
  * @return spectrum value for BLE signal
  */
  void CreateTxPowerSpectralDensity (uint32_t channeloffset, double power);

  /**
   * Update the BER for all receiving transmissions based on latest information 
   */
  void UpdateBer (void);
};


} // namespace ns3

#endif /* BLE_PHY_H */
