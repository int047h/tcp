// Copyright© 2022, Ted Pena <https://github.com/int047h>
// All rights reserved.
//
// This file is part of tcp <https://github.com/int047h/tcp>
#include <tcp/tcp.hpp>

#include <cstdio>

int main()
{
	tcp::startup();

	{ // tcp::Socket needs to be scoped which ensures it's dtor is called
	  // prior to tcp::shutdown
		tcp::Endpoint const endpoint = tcp::Endpoint::lookup("www.google.co.uk", 80);
		tcp::Socket const socket = tcp::Socket::create();

		if (socket.connect(endpoint))
		{
		}
	}

	tcp::shutdown();
	return 0;
}
