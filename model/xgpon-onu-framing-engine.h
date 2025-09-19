/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University College Cork (UCC), Ireland
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
 * Author: Xiuchao Wu <xw2@cs.ucc.ie>
 * Author: Jerome Arokkiam <jerome.arokkia@bt.com>
 */

#ifndef XGPON_ONU_FRAMING_ENGINE_H
#define XGPON_ONU_FRAMING_ENGINE_H

#include "xgpon-onu-engine.h"

#include "xgpon-xgtc-bwmap.h"
#include "xgpon-xgtc-ds-frame.h"
#include "xgpon-xgtc-us-burst.h"



namespace ns3 {


class XgponOnuFramingEngine : public XgponOnuEngine
{
public:
  ///////////////////////////////////////////////////////Required by NS-3
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
	

  const static uint8_t BASE_GRANT_SIZE_XGPON = 4;         //unit: Bytes, ja:update:xgspon

  /**
   * \brief Constructor
   */
  XgponOnuFramingEngine ();
  virtual ~XgponOnuFramingEngine ();


  /**
   * \brief Process the downstream XGTC frame from OLT. It processes BW-MAP, ploam, and XGEM frames through different engines.
   * \param frame the XGTC downstream frame to be processed.
   */
  void ParseXgtcDownstreamFrame (XgponXgtcDsFrame& frame);



  /**
   * \brief produce the upstream burst according to the BW_MAP.
   * \param usBurst the burst to be composed.
   * \param map the BW_MAP in which this burst is scheduled.
   * \param first the index of the first bandwidth allocation that belongs to this burst
   */
  void ProduceXgtcUsBurst (XgponXgtcUsBurst& usBurst, const Ptr<XgponXgtcBwmap>& map, uint16_t first);

private:
  uint8_t m_baseGrantSize; //unit: Bytes ja:update:xgspon
};



}// namespace ns3

#endif // XGPON_ONU_FRAMING_ENGINE_H
