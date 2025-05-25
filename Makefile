ifndef $(dev)
	dev=sample-dev
endif

ifndef $(ip4)
	ip4=172.31.0.2
	ip4_tuple=$(subst ., ,$(ip4))
	ip4_last=$(word 4,$(ip4_tuple))
	ip4_last_next=$(shell expr $(ip4_last) + 1)
	ip4_to_ping=$(subst .$(ip4_last),.$(ip4_last_next),$(ip4))
endif

ifndef $(ip6)
	ip6=fc00::2
	ip6_tuple=$(subst ::, ,$(ip6))
	ip6_last=$(word 2,$(ip6_tuple))
	ip6_last_next=$(shell expr $(ip6_last) + 1)
	ip6_to_ping=$(subst ::$(ip6_last),::$(ip6_last_next),$(ip6))
endif

tun: util run-tun ip ping4 ping6 kill clean
	@echo Testing tun device complete!

tap: util run-tap ip arp ping4 ping6 kill clean
	@echo Testing tap device complete!

util: util.c
	gcc -Wall -x c -o util util.c

run-tun:
	./run.sh $(dev)

run-tap:
	./run.sh $(dev) tap

kill:
	kill $(shell cat /tmp/tun-tap-sample-util.pid)
	rm -f /tmp/tun-tap-sample-util.pid

ping4:
	ping -4 -c 10 $(ip4_to_ping)

ping6:
	ping -6 -c 10 $(ip6_to_ping)

ip:
	ip addr add $(ip4)/31 dev $(dev) && ip addr add $(ip6)/127 dev $(dev) && ip link set up dev $(dev)

.PHONY: arp
arp:
	arp -s $(ip4_to_ping) $(shell ip a | grep -A 3 $(dev) | grep link/ether | awk '{print $$2}') -i $(dev) && ip -6 neighbour add $(ip6_to_ping) lladdr $(shell ip a | grep -A 3 $(dev) | grep link/ether | awk '{print $$2}') dev $(dev)

rm-util: util
	rm -f ./util

rm-nohup: nohup.out
	rm -f ./nohup.out

clean: rm-util rm-nohup

format:
	clang-format -i util.c
