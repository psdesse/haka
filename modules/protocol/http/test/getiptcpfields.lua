-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

require("protocol/ipv4")
require("protocol/tcp")
require("protocol/http")

haka.rule {
	hooks = { "tcp-connection-new" },
	eval = function (self, pkt)
		if pkt.tcp.dstport == 80 then
			pkt.next_dissector = "http"
		end
	end
}

haka.rule {
	hooks = { "http-request" },
	eval = function (self, http)
		print(string.format("Ip source %s port source %s", http.connection.ipsrc, http.connection.prtsrc))
	end
}

