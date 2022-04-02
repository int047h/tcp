// Copyright© 2021, Ted Pena <https://github.com/int047h>
// All rights reserved.
//
// This file is part of tcp <https://github.com/int047h/tcp>
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <array>
#include <string_view>

// Platform dependencies
#ifdef _WIN32
# ifndef _WINSOCKAPI_
#  define _WINSOCKAPI_
# endif  // _WINSOCKAPI_
# include <WinSock2.h>
# include <WS2tcpip.h>
# pragma comment(lib, "Ws2_32.lib")
#else
static_assert(false, // TODO:
			  "Linux/Unix/OSX support for this library is yet to be implemented");
#endif  // _WIN32

namespace tcp {
namespace internal {
/// @brief Swap endianness of integral between big-endian and little-endian
[[nodiscard]] constexpr auto swapBytes(std::integral auto const value) noexcept
{
	constexpr std::size_t kSize = sizeof(value);
	if constexpr (kSize == 1)
		return value;
	else if constexpr (kSize == 2)
		return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00);
	else if constexpr (kSize == 4)
		return ((value >> 24) & 0x000000FF) | ((value >>  8) & 0x0000FF00) |
		       ((value <<  8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
	else
		return ((value >> 56) & 0x00000000000000FF) | ((value >> 40) & 0x000000000000FF00) |
		       ((value >> 24) & 0x0000000000FF0000) | ((value >>  8) & 0x00000000FF000000) |
		       ((value <<  8) & 0x000000FF00000000) | ((value << 24) & 0x0000FF0000000000) |
		       ((value << 40) & 0x00FF000000000000) | ((value << 56) & 0xFF00000000000000);
}
}  // namespace internal

/// @brief Initialises use of this library; must to be called prior to use
inline bool startup() noexcept
{
#ifdef _WIN32
	WSAData data{};
	return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
	return true;
#endif  // _WIN32
}
/// @brief Deinitialises this library
inline void shutdown() noexcept
{
#ifdef _WIN32
	::WSACleanup();
#endif  // _WIN32
}

struct Endpoint
{
	/// @brief Resolve endpoint through hostname lookup
	///
	/// @param host Hostname
	/// @param port Port
	[[nodiscard]] static Endpoint lookup(std::string_view const host, std::uint16_t const port) noexcept
	{
		addrinfo *result{}, hints{.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
		if (::getaddrinfo(host.data(), nullptr, &hints, &result) != 0)
			return {};

		// Use the first result
		const in_addr address = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;

		::freeaddrinfo(result);
		return Endpoint{address, port};
	}
	/// @brief Construct endpoint from IPv4 address in text representation
	///
	/// @param ip IPv4 address in text representation
	/// @param port Port
	[[nodiscard]] static Endpoint derive(std::string_view const ip, std::uint16_t const port) noexcept
	{
		in_addr address{}; 
		return ::inet_pton(AF_INET, ip.data(), &address) == 1 
			? Endpoint{address, port} : Endpoint{};
	}

	constexpr Endpoint() = default;
	constexpr explicit Endpoint(sockaddr_in const &endpoint) noexcept: m_endpoint{endpoint} {}

	/// @brief Construct endpoint from a binary address and port
	///
	/// @param address Host-order address
	/// @param port Host-order port
	constexpr explicit Endpoint(std::uint32_t const address, std::uint16_t const port) noexcept: 
		m_endpoint{AF_INET, internal::swapBytes(port), internal::swapBytes(address)} {}

	/// @brief Construct endpoint from a platform address and port
	///
	/// @param address Platform address
	/// @param port Host-order port
	constexpr explicit Endpoint(in_addr const address, std::uint16_t const port) noexcept:
		m_endpoint{AF_INET, internal::swapBytes(port), address} {}

	/// @brief Test validity of endpoint
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return m_endpoint.sin_family == AF_INET; }

	/// @brief Compare the address of this and another endpoint
	/// @note Only compares address; ignores port
	[[nodiscard]] constexpr bool operator==(Endpoint const &right) const noexcept { return m_endpoint.sin_addr.s_addr == right.m_endpoint.sin_addr.s_addr; }

	/// @brief Access sockaddr storage
	[[nodiscard]] constexpr auto &raw() noexcept { return reinterpret_cast<sockaddr&>(m_endpoint); }
	/// @brief Access const sockaddr storage
	[[nodiscard]] constexpr auto const &raw() const noexcept { return reinterpret_cast<const sockaddr&>(m_endpoint); }

	/// @brief Set address of endpoint
	constexpr void setAddress(std::uint32_t const address) noexcept { m_endpoint.sin_addr.s_addr = internal::swapBytes(address); }
	/// @brief Set port of endpoint
	constexpr void setPort(std::uint16_t const port) noexcept { m_endpoint.sin_port = internal::swapBytes(port); }

	/// @brief Access address of endpoint
	[[nodiscard]] constexpr std::uint32_t address()  const noexcept { return internal::swapBytes(m_endpoint.sin_addr.s_addr); }
	/// @brief Access port of endpoint
	[[nodiscard]] constexpr std::uint16_t port() const noexcept { return internal::swapBytes(m_endpoint.sin_port); }

	/// @brief Convert address into IPv4 text representation
	///
	/// @return x.x.x.x (x <= x < 256)
	[[nodiscard]] std::array<char, 16> ip() const noexcept
	{
		std::array<char, 16> buf{};
		::inet_ntop(AF_INET, &m_endpoint.sin_addr, buf.data(), buf.size());
		return buf;
	}

private:
	sockaddr_in m_endpoint{};
};
struct Socket
{
	/// @brief Returns IPv4/TCP socket
	[[nodiscard]] static Socket create() noexcept
	{
		return Socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
	}

	constexpr Socket() = default;
	constexpr explicit Socket(SOCKET const socket) noexcept: m_socket{socket} {}

	~Socket() noexcept
	{
		if (bool(*this))
			close();
	}

	/// @brief Non copy-constrictible
	Socket(Socket const&) = delete;
	/// @brief Non copy-assignable
	Socket &operator=(Socket const&) = delete;

	/// @brief Move-construction
	Socket(Socket &&right) noexcept
	{
		*this = std::move(right); // Delegate to move-assignment
	}
	/// @brief Move-assignment
	Socket &operator=(Socket &&right) noexcept
	{
		if (this != &right)
		{
			close();
			m_socket = right.release();
		}
		return *this;
	}

	/// @brief Test validity of socket
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return m_socket != INVALID_SOCKET; }

	/// @brief Release semantic ownership of socket
	[[nodiscard]] constexpr SOCKET release() noexcept
	{
		return std::exchange(m_socket, INVALID_SOCKET);
	}
	/// @brief Invalidate socket
	void close() noexcept
	{
		::closesocket(release());
	}

	/// @brief Send data along connected socket
	/// 
	/// @param data Reference to data, size is auto-deduced
	/// @return Number of bytes sent
	std::size_t send(auto const &data) const noexcept
	{
		return ::send(m_socket, reinterpret_cast<const char*>(&data), sizeof(data), 0);
	}
	/// @brief Send data along connected socket
	/// 
	/// @param data Pointer to data
	/// @param size Size of data 
	/// @return Number of bytes sent
	std::size_t send(auto const *data, std::size_t const size) const noexcept
	{
		return ::send(m_socket, reinterpret_cast<const char*>(data), size, 0);
	}

	/// @brief Receive data from connected socket
	/// 
	/// @param data Reference to buffer, size is auto-deduced
	/// @return Number of bytes received
	std::size_t receive(auto &data) const noexcept
	{
		return ::recv(m_socket, reinterpret_cast<char*>(&data), sizeof(data), 0);
	}
	/// @brief Receive data from connected socket
	/// 
	/// @param data Pointer to buffer
	/// @param size Size of buffer 
	/// @return Number of bytes received
	std::size_t receive(auto *data, std::size_t const size) const noexcept
	{
		return ::recv(m_socket, reinterpret_cast<char*>(data), size, 0);
	}

	/// @brief Bind to local endpoint
	bool bind(Endpoint const &endpoint) const noexcept
	{
		return ::bind(m_socket, &endpoint.raw(), sizeof(endpoint.raw())) == 0;
	}
	/// @brief Connect to remote endpoint
	bool connect(Endpoint const &endpoint) const noexcept
	{
		return ::connect(m_socket, &endpoint.raw(), sizeof(endpoint.raw())) == 0;
	}
	/// @brief Allow socket to listen for incoming connections
	bool listen(int const backlog = SOMAXCONN) const noexcept
	{
		return ::listen(m_socket, backlog) == 0;
	}
	/// @brief Permit incoming connection attempt
	/// 
	/// @param socket Connection socket
	/// @param endpoint Connection endpoint
	bool accept(Socket &socket, Endpoint &endpoint) const noexcept
	{
		int endpointSize{sizeof(endpoint.raw())};
		socket = Socket{::accept(m_socket, &endpoint.raw(), &endpointSize)};
		return !!socket; // Explicit cast
	}
	/// @brief Sets the blocking mode of the socket
	///
	/// @param block Should socket operations block?
	bool blocking(bool const block = true) const noexcept
	{
		unsigned long mode = block ? 0 : 1;
		return ::ioctlsocket(m_socket, FIONBIO, &mode) == 0;
	}

private:
	SOCKET m_socket{INVALID_SOCKET};
};
}  // namespace tcp
