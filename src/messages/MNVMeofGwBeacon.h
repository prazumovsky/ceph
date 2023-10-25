// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2023
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_NVMEOFGWBEACON_H
#define CEPH_NVMEOFGWBEACON_H

#include <cstddef>
#include "messages/PaxosServiceMessage.h"
#include "mon/MonCommand.h"
#include "mon/NVMeofGwMap.h"

#include "include/types.h"

typedef GW_STATES_PER_AGROUP_E SM_STATE[MAX_SUPPORTED_ANA_GROUPS];

std::ostream& operator<<(std::ostream& os, const SM_STATE value) {
    os << "SM_STATE [ ";
    for (int i = 0; i < MAX_SUPPORTED_ANA_GROUPS; i++) {
      switch (value[i]) {
          case GW_STATES_PER_AGROUP_E::GW_IDLE_STATE: os << "IDLE "; break;
          case GW_STATES_PER_AGROUP_E::GW_STANDBY_STATE: os << "STANDBY "; break;
          case GW_STATES_PER_AGROUP_E::GW_ACTIVE_STATE: os << "ACTIVE "; break;
          case GW_STATES_PER_AGROUP_E::GW_BLOCKED_AGROUP_OWNER: os << "BLOCKED_AGROUP_OWNER "; break;
          case GW_STATES_PER_AGROUP_E::GW_WAIT_FAILBACK_PREPARED: os << "WAIT_FAILBACK_PREPARED "; break;
          default: os << "Invalid";
      }
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const GW_AVAILABILITY_E value) {
  switch (value) {

        case GW_AVAILABILITY_E::GW_CREATED: os << "CREATED"; break;
        case GW_AVAILABILITY_E::GW_AVAILABLE: os << "AVAILABLE"; break;
        case GW_AVAILABILITY_E::GW_UNAVAILABLE: os << "UNAVAILABLE"; break;

        default: os << "Invalid";
    }
    return os;
}

class MNVMeofGwBeacon final : public PaxosServiceMessage {
private:
  static constexpr int HEAD_VERSION = 1;
  static constexpr int COMPAT_VERSION = 1;

protected:
    //bool                    ana_state[MAX_SUPPORTED_ANA_GROUPS]; // real ana states per ANA group for this GW :1- optimized, 0- inaccessible
    std::string              gw_id;
    SM_STATE                 sm_state;                             // state machine states per ANA group
    uint16_t                 opt_ana_gid;                          // optimized ANA group index as configured by Conf upon network entry, note for redundant GW it is FF
    GW_AVAILABILITY_E        availability;                         // in absence of  beacon  heartbeat messages it becomes inavailable

    uint64_t                 version;

public:
  MNVMeofGwBeacon()
    : PaxosServiceMessage{MSG_MNVMEOF_GW_BEACON, 0, HEAD_VERSION, COMPAT_VERSION}
  {}

  MNVMeofGwBeacon(const std::string &gw_id_, 
        const GW_STATES_PER_AGROUP_E (&sm_state_)[MAX_SUPPORTED_ANA_GROUPS],
        const uint16_t& opt_ana_gid_,
        const GW_AVAILABILITY_E  availability_,
        const uint64_t& version_
  )
    : PaxosServiceMessage{MSG_MNVMEOF_GW_BEACON, 0, HEAD_VERSION, COMPAT_VERSION},
      gw_id(gw_id_), opt_ana_gid(opt_ana_gid_),
      availability(availability_), version(version_)
  {
      for (int i = 0; i < MAX_SUPPORTED_ANA_GROUPS; i++)
        sm_state[i] = sm_state_[i];
  }

  const std::string& get_gw_id() const { return gw_id; }
  const uint16_t& get_opt_ana_gid() const { return opt_ana_gid; }
  const GW_AVAILABILITY_E& get_availability() const { return availability; }
  const uint64_t& get_version() const { return version; }
  const SM_STATE& get_sm_state() const { return sm_state; };

private:
  ~MNVMeofGwBeacon() final {}

public:

  std::string_view get_type_name() const override { return "nvmeofgwbeacon"; }

  void print(std::ostream& out) const override {
    out << get_type_name() << " nvmeofgw" << "("
	    << gw_id <<  ", " << sm_state << "," << opt_ana_gid << "," << availability << "," << version
	    << ")";
  }

  void encode_payload(uint64_t features) override {
    header.version = HEAD_VERSION;
    header.compat_version = COMPAT_VERSION;
    using ceph::encode;
    paxos_encode();
    encode(gw_id, payload);
    for (int i = 0; i < MAX_SUPPORTED_ANA_GROUPS; i++)
      encode((int)sm_state[i], payload);
    encode(opt_ana_gid, payload);
    encode((int)availability, payload);
    encode(version, payload);
  }

  void decode_payload() override {
    using ceph::decode;
    auto p = payload.cbegin();
    
    paxos_decode(p);
    decode(gw_id, payload);
    for (int i = 0; i < MAX_SUPPORTED_ANA_GROUPS; i++) {
      int e; decode(e, payload);
      sm_state[i] = static_cast<GW_STATES_PER_AGROUP_E>(e);
    }
    decode(opt_ana_gid, p);
    int a; decode(a, p);
    availability = static_cast<GW_AVAILABILITY_E>(a);
    decode(version, p);  
  }

private:
  template<class T, typename... Args>
  friend boost::intrusive_ptr<T> ceph::make_message(Args&&... args);
};


#endif