//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: simple network client for the HwMonitorService
//======================================================================================================================

#include "HwMonitorClient.hpp"

#include "Essential.hpp"

#include "Protocol.hpp"
#include "BinaryStream.hpp"
using own::BinaryOutputStream;
using own::BinaryInputStream;
#include "Socket.hpp"
using own::TcpSocket;
using own::SocketError;
#include "SystemErrorInfo.hpp"
using own::getLastError;
using own::getErrorString;
#include "LangUtils.hpp"
using fut::make_unique;
#include "ContainerUtils.hpp"
using own::span;
using own::make_span;
#include "CriticalError.hpp"

#include <string>
using std::string;
#include <vector>
using std::vector;
#include <array>
using std::array;
#include <chrono>
using std::chrono::milliseconds;


namespace hwmon {


//======================================================================================================================
//  enum to string conversion

const char * enumString( ConnectStatus status ) noexcept
{
	static const char * const ConnectStatusStr [] =
	{
		"The operation was successful.",
		"Operation failed because underlying networking system could not be initialized.",
		"Connect operation failed because the socket is already connected.",
		"The hostname you entered could not be resolved to IP address.",
		"Could not connect to the target server, either it's down or the port is closed.",
		"Other system error.",
		"Internal error of this library. Please create a github issue.",
	};
	static_assert( size_t(ConnectStatus::UnexpectedError) + 1 == fut::size(ConnectStatusStr), "update the ConnectStatusStr" );

	if (size_t(status) < fut::size(ConnectStatusStr))
	{
		return ConnectStatusStr[ size_t(status) ];
	}
	else
	{
		return "<invalid status>";
	}
}

const char * enumString( RequestStatus status ) noexcept
{
	static const char * const RequestStatusStr [] =
	{
		"The request was succesful.",
		"Request failed because the client is not connected.",
		"Failed to send the request message.",
		"Server has closed the connection.",
		"No reply has arrived from the server in given timeout.",
		"There has been some other error while trying to receive a reply.",
		"The reply from the server is invalid.",
		"Such sensor was not found.",
		"Sensor is available but not monitored.",
		"Sensor is available and monitored, but reading its value has failed.",
		"Internal error of this library. Please create a github issue.",
	};
	static_assert( size_t(RequestStatus::UnexpectedError) + 1 == fut::size(RequestStatusStr), "update the RequestStatusStr" );

	if (size_t(status) < fut::size(RequestStatusStr))
	{
		return RequestStatusStr[ size_t(status) ];
	}
	else
	{
		return "<invalid status>";
	}
}


//======================================================================================================================
//  Client: main API

Client::Client(  ) noexcept : _socket( new TcpSocket ) {}

Client::~Client() noexcept {}

bool Client::isConnected() const noexcept
{
	return _socket->isConnected();
}

ConnectStatus Client::connect( const std::string & host, uint16_t port ) noexcept
{
	SocketError connectRes = _socket->connect( host, port );
	if (connectRes != SocketError::Success)
	{
		switch (connectRes)
		{
			case SocketError::AlreadyConnected:      return ConnectStatus::AlreadyConnected;
			case SocketError::NetworkingInitFailed:  return ConnectStatus::NetworkingInitFailed;
			case SocketError::HostNotResolved:       return ConnectStatus::HostNotResolved;
			case SocketError::ConnectFailed:         return ConnectStatus::ConnectFailed;
			default:                                 return ConnectStatus::OtherSystemError;
		}
	}

	// rather set some default timeout for recv operations, user can always override this
	_socket->setTimeout( milliseconds( 500 ) );

	return ConnectStatus::Success;
}

bool Client::disconnect() noexcept
{
	SocketError status = _socket->disconnect();
	if (status == SocketError::Success)
		return true;
	else if (status == SocketError::NotConnected)
		return false;
	else
		// This can happen if the client doesn't respond to a server's FIN packet by calling close() in time and the
		// server just forcibly ends the connection producing a network error.
		// In this case, the user doesn't need to know because he wanted the socket closed and that's what's gonna happen.
		return true;
}

bool Client::setTimeout( std::chrono::milliseconds timeout ) noexcept
{
	// Currently we cannot set timeout on a socket that is not connected, because the actual system socket is created
	// during connect operation, so the preceeding setTimeout calls would go to nowhere.
	if (!_socket->isConnected())
	{
		return false;
	}

	return _socket->setTimeout( timeout );
}

SensorReadResult Client::requestSensorReading( const std::string & sensorID ) noexcept
{
	if (!_socket->isConnected())
	{
		return { RequestStatus::NotConnected, 0 };
	}

	SensorReadResult result = { RequestStatus::UnexpectedError, 0.0f };

	vector< uint8_t > requestData = own::toByteVector( SensorRequest( sensorID ) );

	auto sendRes = _socket->send( requestData );
	if (sendRes != SocketError::Success)
	{
		result.status = RequestStatus::SendRequestFailed;
		return result;
	}

	std::vector< uint8_t > responseData;
	SocketError headerStatus = _socket->receiveOnce( responseData );
	if (headerStatus != SocketError::Success)
	{
		if (headerStatus == SocketError::ConnectionClosed)
			result.status = RequestStatus::ConnectionClosed;
		else if (headerStatus == SocketError::Timeout)
			result.status = RequestStatus::NoReply;
		else
			result.status = RequestStatus::ReceiveError;
		return result;
	}
	else if (responseData.size() < 4)
	{
		result.status = RequestStatus::InvalidReply;
		return result;
	}

	SensorResponse response;
	if (!own::fromBytes( responseData, response ))
	{
		result.status = RequestStatus::InvalidReply;
		return result;
	}

	if (response.code != ResponseCode::Success)
	{
		if (response.code == ResponseCode::InvalidRequest)
			result.status = RequestStatus::UnexpectedError;
		else if (response.code == ResponseCode::SensorNotFound)
			result.status = RequestStatus::SensorNotFound;
		else if (response.code == ResponseCode::SensorNotMonitored)
			result.status = RequestStatus::SensorNotMonitored;
		else if (response.code == ResponseCode::SensorFailed)
			result.status = RequestStatus::SensorFailed;
		return result;
	}

	result.status = RequestStatus::Success;
	result.sensorValue = response.value;
	return result;
}

system_error_t Client::getLastSystemError() const noexcept
{
	return _socket->getLastSystemError();
}

string Client::getLastSystemErrorStr() const noexcept
{
	return getErrorString( getLastSystemError() );
}


//======================================================================================================================


} // namespace hwmon
