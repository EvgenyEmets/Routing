#include "Routing.hpp"

#include "PacketParser.hpp"
#include "api/Packet.hpp"
#include <runos/core/logging.hpp>

#include <sstream>

namespace runos {

REGISTER_APPLICATION(Routing, {"controller", "host-manager", "switch-manager", "topology", "link-discovery", ""})

void Routing::init(Loader* loader, const Config& config)
{
    switch_manager_ = SwitchManager::get(loader);
    host_manager_ = HostManager::get(loader);
    link_discovery_ = LinkDiscovery::get(loader);

    connect(switch_manager_, &SwitchManager::switchUp, this, &Routing::onSwitchUp);
    connect(switch_manager_, &SwitchManager::switchDown, this, &Routing::onSwitchDown);
    connect(host_manager_, &HostManager::hostDiscovered, this, &Routing::onHostDiscovered);
    connect(switch_manager_, &SwitchManager::linkhUp, this, &Routing::onLinkUp);
    connect(switch_manager_, &SwitchManager::linkDown, this, &Routing::onLinkDown);
    connect(link_discovery_, &LinkDiscovery::linkDiscovered, this, &Routing::onLinkDiscovered);

    auto data_base = std::make_shared<HostsDatabase>();

    handler_ = Controller::get(loader)->register_handler(
    [=](of13::PacketIn& pi, OFConnectionPtr ofconn) mutable -> bool
    {
        LOG(INFO) << "NEW PACKET";
        PacketParser pp(pi);
        runos::Packet& pkt(pp);

        src_mac_ = pkt.load(ofb::eth_src);
        dst_mac_ = pkt.load(ofb::eth_dst);
        in_port_ = pkt.load(ofb::in_port);
        dpid_ = ofconn->dpid();
        eth_type_ = pkt.load(ofb::eth_type);
        bool in_db = false;
        if (eth_type_ == 0x0800) {
            ip_src_ = pkt.load(ofb::ip_src);
            ip_dst_ = pkt.load(ofb::ip_dst);
            //in_db = data_base->setPort(dpid_, src_mac_, in_port_);
        } else if (eth_type_ == 0x0806) {
            LOG(INFO) << "ARP";
            arp_spa_ = pkt.load(ofb::arp_spa);
            arp_tpa_ = pkt.load(ofb::arp_tpa);
            LOG(INFO) << "src: " << arp_spa_;
            LOG(INFO) << "dst: " << arp_tpa_;
        }
        
        if (not data_base->setPort(dpid_,
                                    src_mac_,
                                    in_port_)) {
            return false;
        }
        
        auto target_port = data_base->getPort(dpid_, dst_mac_);
        if (target_port != boost::none) {
            send_unicast(*target_port, pi);

        } else {
            send_broadcast(pi);
        }

        return true;
    }, -5);
}

void Routing::onSwitchUp(SwitchPtr sw) {
    table_[sw->dpid()]->first[0x00000000] = 0;
    table_[sw->dpid()]->second[0] = 0;

    of13::FlowMod fm;
    fm.command(of13::OFPFC_ADD);
    fm.table_id(0);
    fm.priority(2);
    of13::ApplyActions applyActions;
    fm.add_oxm_field(new of13::EthType(0x0806));
    of13::OutputAction output_action(of13::OFPP_CONTROLLER, 0xFFFF);
    applyActions.add_action(output_action);
    fm.add_instruction(applyActions);
    sw->connection()->send(fm);
    
    /*of13::FlowMod fm1;
    fm1.command(of13::OFPFC_ADD);
    fm1.table_id(0);
    fm1.priority(2);
    of13::ApplyActions applyActions1;
    fm1.add_oxm_field(new of13::EthType(0x0800));
    of13::OutputAction output_action1(of13::OFPP_CONTROLLER, 0xFFFF);
    applyActions1.add_action(output_action1);
    fm1.add_instruction(applyActions1);
    sw->connection()->send(fm1);*/

    of13::FlowMod fm2;
    fm2.command(of13::OFPFC_ADD);
    fm2.table_id(0);
    fm2.priority(1);
    of13::ApplyActions applyActions2;
    fm2.add_oxm_field(new of13::EthType(0x0800));
    of13::OutputAction output_action2(of13::OFPP_CONTROLLER, 0xFFFF);
    applyActions2.add_action(output_action2);
    fm2.add_instruction(applyActions2);
    sw->connection()->send(fm2);
}

void Routing::onLinkUp(PortPtr pp) {
    auto TPtr = table_.find(pp->switch_()->dpid());
    if (TPtr != table_.end()) {}
}

void Routing::onLinkDiscovered(switch_and_port from, switch_and_port to) {
}   

void Routing::send_unicast(uint32_t target_port, const of13::PacketIn& pi) {
}

    { // Send PacketOut.

    of13::PacketOut po;
    po.data(pi.data(), pi.data_len());
    of13::OutputAction output_action(target_port, of13::OFPCML_NO_BUFFER);
    po.add_action(output_action);
    switch_manager_->switch_(dpid_)->connection()->send(po);

    } // Send PacketOut.

    { // Create FlowMod.

    of13::FlowMod fm;
    fm.command(of13::OFPFC_ADD);
    fm.table_id(0);
    fm.priority(1);
    std::stringstream ss;
    fm.idle_timeout(uint64_t(60));
    fm.hard_timeout(uint64_t(1800));

    ss.str(std::string());
    ss.clear();
    ss << src_mac_;
    fm.add_oxm_field(new of13::EthSrc{
            fluid_msg::EthAddress(ss.str())});
    ss.str(std::string());
    ss.clear();
    ss << dst_mac_;
    fm.add_oxm_field(new of13::EthDst{
            fluid_msg::EthAddress(ss.str())});

    of13::ApplyActions applyActions;
    of13::OutputAction output_action(target_port, of13::OFPCML_NO_BUFFER);
    applyActions.add_action(output_action);
    fm.add_instruction(applyActions);
    switch_manager_->switch_(dpid_)->connection()->send(fm);

    } // Create FlowMod.
}

void Routing::send_broadcast(const of13::PacketIn& pi)
{
    of13::PacketOut po;
    po.data(pi.data(), pi.data_len());
    po.in_port(in_port_);
    of13::OutputAction output_action(of13::OFPP_ALL, of13::OFPCML_NO_BUFFER);
    po.add_action(output_action);
    switch_manager_->switch_(dpid_)->connection()->send(po);
}

bool HostsDatabase::setPort(uint64_t dpid,
                            ethaddr mac,
                            uint32_t in_port)
{
    if (is_broadcast(mac)) {
        LOG(WARNING) << "Broadcast source address, dropping";
        return false;
    }

    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    seen_ports_[dpid][mac] = in_port;
    return true;
}

boost::optional<uint32_t> HostsDatabase::getPort(uint64_t dpid, ethaddr mac)
{
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = seen_ports_[dpid].find(mac);
    
    if (it != seen_ports_[dpid].end()) {
        return it->second;
    
    } else {
        return boost::none;
    }
}

bool MacIP::setConform(ethaddr mac, uint32_t ip) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    conform_[ip] = mac;
    return true;
}

ethaddr MacIP::getMac(uint32_t ip) {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return conform_[ip];
}

} // namespace runos
