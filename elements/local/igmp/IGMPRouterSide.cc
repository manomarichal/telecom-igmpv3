//
// Created by Gebruiker on 7/04/2021.
//
#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/ipaddress.hh>
#include <clicknet/ip.h>
#include "IGMPRouterSide.hh"

CLICK_DECLS
IGMPRouterSide::IGMPRouterSide() : _timer(this){}

IGMPRouterSide::~IGMPRouterSide() {}


/**
 * configures the routerside object, only takes therouter ip or interface as input
 * @param conf standard configure parameter, click practice
 * @param errh standard configure parameter, click practice
 * @return returns -1 on failed read and 0 on completed read
 */
int IGMPRouterSide::configure(Vector <String> &conf, ErrorHandler *errh) {
    //cunfugure like RFC 8.14
    if (Args(conf, this, errh).read_mp("ROUTERADDRESS", routerIP).read_mp("RV", robustness_variable)
                .read_mp("QI", query_interval).read_mp("MRT", max_response_time).complete() < 0) {
        click_chatter("failed read when initialising router, returning 0");
        if(0<robustness_variable<7){
            return  -1;
        }
        return -1;
    }
   // click_chatter("initialising routeraddress %s", routerIP.unparse().c_str());
    for (int i=0; i < port_count()[2] - 48; i++)
    {
        interface_states.push_back(Vector<igmp_group_state>());
    }
    _timer.initialize(this);
    _timer.schedule_after_sec(1);
    return 0;
}


/**
 * forward packets to interfacest that are interested
 * @param p the packet to multicast
 * @param port the input port
 */
void IGMPRouterSide::multicast_udp_packet(Packet *p, int port) {
    const click_ip *ip_header = p->ip_header();
    //click_chatter("hlen, %i", length);
    for (int i = 0; i < interface_states.size(); i++) {
        for (igmp_group_state state: interface_states[i]) {
            if (state.multicast_adress == ip_header->ip_dst) {
                //click_chatter("multicasting on interface %i, port %i", i, i+3);
                Packet *package = p->clone();
                output(i + 3).push(package);
            }
        }
    }

}


/**
 * TODO: implemet this
 * @param ip_header click ip struct of the obtained message
 * @param state group state
 * @param record group record
 */
/*
void IGMPRouterSide::update_group_state(const click_ip *ip_header, igmp_group_state state, igmp_group_record record) {
    // add client to group state if it is not in it
    //click_chatter("updating groupstate %s", state.multicast_adress.unparse().c_str());


    bool already_in = false;
    for (IPAddress client: state.clients)
    {
        if (client == ip_header->ip_src)
        {
            already_in = true;
        }
    }
    if (!already_in)
    {
        click_chatter("adding %s to group %s", IPAddress(ip_header->ip_src).unparse().c_str(), state.multicast_adress.unparse().c_str());
        state.clients.push_back(IPAddress(ip_header->ip_src));
    }
     */

    /* idk what to do here yet
    // update client mode
    if (record.record_type == IGMP_V3_CHANGE_TO_INCLUDE)
    {
        state.mode = IGMP_V3_INCLUDE;
    }
    else if (record.record_type == IGMP_V3_CHANGE_TO_EXCLUDE)
    {
        state.mode = IGMP_V3_EXCLUDE;
    }

}
*/
/**
 * function to update group states when receiving new igmp message, not fully implemented like the RFC
 * this is still TODO:
 * @param ip_header click ip struct of the obtained message
 * @param group_records vector containing group records of the message
 */
void IGMPRouterSide::update_group_states(const click_ip *ip_header, Vector <igmp_group_record> group_records, int port) {
    bool VERBOSE = false;

    if (VERBOSE)
    {
        click_chatter("updating group states in interface %i", port);
    }
    for (igmp_group_record record:group_records) {
        bool exists = false;
        for (int i = 0; i<interface_states[port].size();i++) {
            if (record.multicast_adress == interface_states[port][i].multicast_adress) {
                interface_states[port][i].mode = record.record_type;
//                click_chatter("====state========= %d", interface_states[port][i].mode);
                exists = true;
            }
        }

            // if no group state exists for the multicast adress, make new one
        if (!exists) {
            igmp_group_state new_state;
            new_state.mode = record.record_type;
            new_state.multicast_adress = record.multicast_adress;
            interface_states[port].push_back(new_state);
            if (VERBOSE) {
                click_chatter("creating new group state for interface %i", port);
                click_chatter("\t multicast adress: %s", IPAddress(record.multicast_adress).unparse().c_str());
                click_chatter("\t mode: %i", record.record_type);
            }
        }
    }
//    click_chatter("checking if all interface %d still want to accept packages", port);
//    for (int i = 0; i<interface_states[port].size();i++) {
//        click_chatter("mode of port: %i", interface_states[port][i].mode);
//        if (4 == interface_states[port][i].mode or 2 == interface_states[port][i].mode) {
//            click_chatter("port %d still in use", port);
//            continue;
//        }
//        else{
//            click_chatter("removing interface form port %d", port);
//
//            interface_states[port].erase(interface_states[port].begin()+i);
//            click_chatter("size is nw %d ", interface_states[port].size());
//        }
//    }

    }

/**
 * router push function, router can get mulitple types of packets, right now we only look at UDP and igmp v3 membership reports
 * when obtaining an igmp message, the router will add this client to its group state if it was not there already, the packet will be unpacked
 * based on the packets content the router will see if the client want to join or leave. Router will alter group state accordingly
 * if the router gets an UDP packet, it will multicast this to all clients interested.
 * Other messages get pushed to another output port
 * @param port input port of the packet
 * @param p the packet itself
 */
void IGMPRouterSide::push(int port, Packet *p) {

    const click_ip *ip_header = p->ip_header();
    //click_chatter("PACKET, %i, %i", ip_header->ip_p, port);

    if (ip_header->ip_p == IP_PROTO_IGMP) {
        /*
         * input port of the igmp messages
         * ontbind de igmp header
         * kijk na of het een v3 membership report is
         * haal de filter mode uit de igmp header
         * als filtermode exclude is (en de src list leeg is) voer je join uit
         *      voeg toe aan group state, zet timer
         * als filtermode include is (en de src list is leeg) voer je leave uit
         *      voeg toe aan group state, zet timer
         * steeds kijken of deze messages geen repeat messages zijn, deze renewed de timers enkel en moet dus niet opnieuw worden geadd
         */
        //click_chatter("IGMP PACKET type %i, port %i", ip_header->ip_p, port);
        // unpacking data
        const router_alert *alert_ptr = reinterpret_cast<const router_alert *>(ip_header + 1);
        const igmp_mem_report *info_ptr = reinterpret_cast<const igmp_mem_report *>(alert_ptr + 1);
        const igmp_group_record_message *records_ptr = reinterpret_cast<const igmp_group_record_message *>(info_ptr +
                                                                                                           1);

        igmp_mem_report report_info = report_helper->igmp_unpack_info(info_ptr);
        Vector <igmp_group_record> group_records = report_helper->igmp_unpack_group_records(records_ptr,
                                                                                     report_info.number_of_group_records);

        //if the client leaves, a group specific query is sent to the group to check whether there are still listeners
        for (int i = 0; i < group_records.size(); i++) {
            if (group_records[i].record_type == 3){
                Packet *q = make_group_specific_query_packet();
                for(int i = 6; i<9;i++){
                    Packet *package = q->clone();
                    output(i).push(package);
                }
            }
        }
        //click_chatter("when receiving packet, mode is %d", group_records[0].record_type);
        /*
        //receivers will have joined an not yet left
        for (int i = 0; i < group_records.size(); i++) {
            if (group_records[i].record_type == IGMP_V3_EXCLUDE or
                group_records[i].record_type == IGMP_V3_CHANGE_TO_EXCLUDE) {
                receivers.push_back(ip_header->ip_src);
            }
            if (group_records[i].record_type == IGMP_V3_INCLUDE or
                group_records[i].record_type == IGMP_V3_CHANGE_TO_INCLUDE) {
                for (int y = 0; y < receivers.size(); y++) {
                    if (receivers[y] == ip_header->ip_src) {
                        receivers.erase(receivers.begin(), receivers.begin() + y);
                    }
                }
            }
        }
         */
        update_group_states(ip_header, group_records, port);

    } else if (ip_header->ip_p == 17) {
        //click_chatter("UDP PACKET, %i, %i", ip_header->ip_p, port);
        multicast_udp_packet(p, port);
    } else {
        output(port).push(p);
    }
}

/**
 * run timer will be triggered after the _timer gets scheduled
 * the timer gets scheduled by using _timer.schedule_after_sec(time)
 * you can check whether the timer is set by using _timer.scheduled()
 */
void IGMPRouterSide::run_timer(Timer * timer)
{
    _timer.schedule_after_sec(1);
    _local_timer++;
    click_chatter("local timer: %d",_local_timer);
    assert(timer == &_timer);
    if(_local_timer == query_interval){
        Packet *q = make_general_query_packet();
        for(int i = 6; i<9;i++){
            Packet *package = q->clone();
            output(i).push(package);
        }
        _local_timer=0;
    }



}

WritablePacket * IGMPRouterSide::make_general_query_packet()
{

    WritablePacket *p = Packet::make(query_helper->get_size_of_data(0) + sizeof(click_ip) + 4);
    memset(p->data(), 0, p->length()); // erase previous random data on memory requested

    click_ip *ip_header = query_helper->add_ip_header(p, routerIP, ASMC_ADDRESS,false);
    router_alert *r_alert = query_helper->add_router_alert(ip_header + 1);
    ip_header->ip_sum = click_in_cksum((unsigned char *) ip_header, sizeof(click_ip) + sizeof(router_alert));
    query_helper->add_igmp_data(r_alert + 1, Vector <IPAddress>(), IPAddress("0.0.0.0"), false,
                                robustness_variable, query_interval, max_response_time);

    p->set_ip_header(ip_header, sizeof(click_ip));
    p->timestamp_anno().assign_now();
    p->set_dst_ip_anno(IPAddress(ASMC_ADDRESS));
    return p;
}


/*
 * makes group specific query, make sure that the groups have been initialised before making one of these
 */
WritablePacket * IGMPRouterSide::make_group_specific_query_packet()
{
    WritablePacket *p = Packet::make(query_helper->get_size_of_data(0) + sizeof(click_ip) + 4);
    memset(p->data(), 0, p->length()); // erase previous random data on memory requested

    click_ip *ip_header = query_helper->add_ip_header(p, routerIP, ASMC_ADDRESS,true);
    router_alert *r_alert = query_helper->add_router_alert(ip_header + 1);
    ip_header->ip_sum = click_in_cksum((unsigned char *) ip_header, sizeof(click_ip) + sizeof(router_alert));
    bool found = false;
    for (int i = 0; i < interface_states.size(); i++) {
        for (igmp_group_state state: interface_states[i]) {
            if(!found) {
                if (state.multicast_adress) {
                    query_helper->add_igmp_data(r_alert + 1, Vector<IPAddress>(), state.multicast_adress, true,
                                                robustness_variable, query_interval, max_response_time);
                    found = true;
                } else {
                    return NULL;
                }
            }
        }
    }
    p->set_ip_header(ip_header, sizeof(click_ip));
    p->timestamp_anno().assign_now();
    p->set_dst_ip_anno(IPAddress(ASMC_ADDRESS));

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IGMPRouterSide) // forces to create element within click
