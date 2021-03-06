-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

require('protocol/ipv4')

haka.rule{
	hooks = { "ipv4-up" },
	eval = function (self, p)
		pkt = p
		require('alert-doc')
		require('alertupdate-doc')
	end
}
