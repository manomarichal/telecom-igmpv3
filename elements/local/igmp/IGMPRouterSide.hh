#ifndef CLICK_IGMPROUTERSIDE_HH
#define CLICK_IGMPROUTERSIDE_HH
#include <click/element.hh>
#include "IGMPV3ReportHelper.hh"
#include "igmp.h"
CLICK_DECLS

class IGMPRouterSide : public Element {
public:
    IGMPRouterSide();
    ~IGMPRouterSide();

    const char *class_name() const	{ return "IGMPRouterSide"; }
    const char *port_count() const	{ return "3/3"; }
    const char *processing() const	{ return PUSH; }
    int configure(Vector<String>&, ErrorHandler*);
    void push(int, Packet *);

private:
    void update_group_states(const click_ip *ip_header, Vector<igmp_group_record> group_records);
    void update_group_state(const click_ip *ip_header, igmp_group_state state, igmp_group_record record);
    void multicast_packet(Packet *p, int port);
    IPAddress routerIP;
    Vector<igmp_group_state> group_states;
    IGMPV3ReportHelper* helper = new IGMPV3ReportHelper();
    Vector<IPAddress> receivers;
};

CLICK_ENDDECLS
#endif

