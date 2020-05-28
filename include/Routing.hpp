#pragma once

#include "Application.hpp"
#include "Loader.hpp"
#include "SwitchManager.hpp"
#include "Controller.hpp"
#include "HostManager.hpp"
#include "LinkDiscovery.hpp"
#include "api/SwitchFwd.hpp"
#include "oxm/openflow_basic.hh"

#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <unordered_map>

namespace runos {

    using SwitchPtr = safe::shared_ptr<Switch>;
    namespace of13 = fluid_msg::of13;

    namespace ofb {
        constexpr auto in_port = oxm::in_port();
        constexpr auto eth_src = oxm::eth_src();
        constexpr auto eth_dst = oxm::eth_dst();
        constexpr auto eth_type = oxm::eth_type();
        constexpr auto arp_spa = oxm::arp_spa();
        constexpr auto arp_tpa = oxm::arp_tpa();
        constexpr auto ip_src = oxm::ipv4_src();
        constexpr auto ip_dst = oxm::ipv4_dst();
    }

    class Routing : public Application {
        Q_OBJECT
        SIMPLE_APPLICATION(Routing, "Routing")
    public:
        void init(Loader* loader, const Config& config) override;

    protected slots:
        void onSwitchUp(SwitchPtr sw);
        void onSwitchDown(SwitchPtr sw);
	    void onLinkDiscovered(switch_and_port from, switch_and_port to);
	    void onLinkUp(PortPtr PORT);
	    void onLinkDown(PortPtr PORT);
        void onHostDiscovered(Host* h);
	

    private:
        OFMessageHandlerPtr handler_;
        SwitchManager* switch_manager_;
        HostManager* host_manager_;
        LinkDiscovery* link_discovery_;

        ethaddr src_mac_;
        ethaddr dst_mac_;
        uint32_t ip_src_;
        uint32_t ip_dst_;
        uint64_t dpid_;
        uint32_t in_port_;
        uint16_t eth_type_;
        uint32_t arp_spa_;
        uint32_t arp_tpa_;
        std::unordered_map<uint64_t, Table_fields> table_;

        void send_unicast(uint32_t target_switch_and_port, const of13::PacketIn& pi);
        void send_broadcast(const of13::PacketIn& pi);
    };

    class HostsDatabase {
    public:
        bool setPort(uint64_t dpid, ethaddr mac, uint32_t in_port);
        boost::optional<uint32_t> getPort(uint64_t dpid, ethaddr mac);
    
    private:
        boost::shared_mutex mutex_;
        std::unordered_map<uint64_t,
                std::unordered_map<ethaddr, uint32_t>> seen_ports_;
    };
    
    class MacIP {
    public:
        bool setConform(ethaddr mac, uint32_t ip);
        ethaddr getMac(uint32_t ip);
    private:
        boost::shared_mutex mutex_;
        std::unordered_map<uint64_t, ethaddr> conform_;
    };

    class Table_fields {
        std::unordered_map<uint32_t, uint32_t> host_table_;
        std::unordered_map<uint32_t, uint32_t> inv_host_table_;
        std::unordered_map<uint64_t, uint32_t> switch_table_;
        std::unordered_map<uint32_t, uint64_t> inv_switch_table_;
    public:
        void addHost(uint32_t IP, uint32_t Port) {
            host_table_[IP] = Port;
            inv_host_table_[Port] = IP;
        }
        void addswitch(uint64_t dpid, uint32_t Port) {
            switch_table_[dpid] = Port;
            inv_switch_table_[Port] = dpid;
        }
        void addPort(uint32_t Port) {
            inv_host_table_[Port] = 0;
            inv_switch_table_[Port] = 0;
        }
        void delPort(uint32_t Port) {
            if (inv_host_table_[Port] != 0) {
                uint32_t host = inv_host_table_[Port];
                inv_host_table_.erase(Port);
                host_table_.erase(host);
            } else if (inv_switch_table_[Port] != 0) {
                uint64_t dpid = inv_switch_table_[Port];
                inv_switch_table_.erase(Port);
                switch_table_.erase(dpid);
            }
        }
    };


} // namespace runos
