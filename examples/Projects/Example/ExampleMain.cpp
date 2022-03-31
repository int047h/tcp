// Copyright© 2021, Ted Pena <https://github.com/int047h>
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
		const auto endpoint = tcp::Endpoint::lookup("www.google.co.uk", 80);
		const auto socket = tcp::Socket::create();

		const std::string get
		{
			"GET index.html\r\n"
			"HTTP/1.1\r\n"
			"Host: www.google.co.uk\r\n"
			"Content-Type: text/plain\r\n\r\n"
		};

		if (socket.connect(endpoint) && socket.send(get.data(), get.size()))
		{
			static std::array<char, 0x1000> buffer{};
			socket.receive(buffer.data(), buffer.size());

			std::printf("%s\n", buffer.data());
		}
	}

	tcp::shutdown();
	return 0;
}
