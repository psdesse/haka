# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

find_package(PCAP)
if(PCAP_FOUND)
	add_library(packet-pcap MODULE main.c)
	set_target_properties(packet-pcap PROPERTIES OUTPUT_NAME pcap)

	include_directories(${PCAP_INCLUDE_DIR})
	target_link_libraries(packet-pcap ${PCAP_LIBRARY})

	INSTALL_MODULE(packet-pcap packet)

	# Test
	# include(TestModuleLoad)
	# TEST_MODULE_LOAD(packet-pcap)
endif()
