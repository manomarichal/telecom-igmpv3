// Output configuration: 
//
// Packets for the network are put on output 0
// Packets for the host are put on output 1

elementclass Client {
	$address, $gateway |
	igmp::IGMPClientSide(CADDR $address, MADDR 224.0.0.22, ASMADDR 224.0.0.1, URI 1, RV 2);

	ip :: Strip(14)                             // get rid of ethernet header
		-> CheckIPHeader()                      // check if ip header is correct
		-> rt :: StaticIPLookup(                // generic routing table, idk hoe ge die update
					$address:ip/32 0,
					$address:ipnet 0,
					0.0.0.0/0.0.0.0 $gateway 1)
		-> [1]output;                            // sends packet to output 1

	rt[1]
		-> DropBroadcasts                   // drops packets that arrived as link-level broadcast or multicast
		-> ipgw :: IPGWOptions($address)    // no idea why this is here
		-> FixIPSrc($address)               // sets source adress to parameter
		-> ttl :: DecIPTTL                  // dec time to live
		-> frag :: IPFragmenter(1500)       // fragments ip packet
		-> arpq :: ARPQuerier($address)
		-> output;

	ipgw[1] -> [0]igmp

	//ipgw[1] -> ICMPError($address, parameterproblem) -> output;         // IPGWOptions error poort
	ttl[1]  -> ICMPError($address, timeexceeded) -> output;             // DecIPTTL error poort
	frag[1] -> ICMPError($address, unreachable, needfrag) -> output;    // IPFragmenter error poort

	// Incoming Packets
	input
	    -> igmpfilter::IPClassifier(ip proto 2, ip proto 17);        //proto is protocol from packet, 2 is if statement trying to match protocol nt 2

	igmpfilter[0]
	    -> [0]igmp;

    igmpfilter[1]
        -> [1]igmp;

    // packets that go to router
	igmp[0]
	    -> EtherEncap(0x0800, $address, $gateway)
        -> ToDump(/home/student/Desktop/output.pcap)
        -> output;

    // packets that go to clients
    igmp[1]
		-> [1]output;

    // others
    igmp[2]
		-> HostEtherFilter($address)
		-> in_cl :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800)
		-> arp_res :: ARPResponder($address)
		-> output;


	in_cl[1] -> [1]arpq;
	in_cl[2] -> ip;
}
