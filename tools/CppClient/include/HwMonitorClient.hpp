//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: simple network client for the HwMonitorService
//======================================================================================================================

#ifndef HWMON_CLIENT_INCLUDED
#define HWMON_CLIENT_INCLUDED


#include "SystemErrorType.hpp"

#include <string>  // system error str
#include <memory>  // unique_ptr<Socket>
#include <chrono>  // timeout

namespace own {
	class TcpSocket;
}


namespace hwmon {


constexpr uint16_t defaultPort = 17748;


//======================================================================================================================

/// All the possible ways how the connect operation can end up
enum class ConnectStatus
{
	Success,                 ///< The operation was successful.
	NetworkingInitFailed,    ///< Operation failed because underlying networking system could not be initialized. Call getLastSystemError() for more info.
	AlreadyConnected,        ///< Connect operation failed because the socket is already connected. Call disconnect() first.
	HostNotResolved,         ///< The hostname you entered could not be resolved to IP address. Call getLastSystemError() for more info.
	ConnectFailed,           ///< Could not connect to the target server, either it's down or the port is closed. Call getLastSystemError() for more info.
	OtherSystemError,        ///< Other system error. Call getLastSystemError() for more info.
	UnexpectedError,         ///< Internal error of this library. This should not happen unless there is a mistake in the code, please create a github issue.
};
const char * enumString( ConnectStatus status ) noexcept;

/// All the possible ways how a request can end up
enum class RequestStatus
{
	Success,            ///< The request was succesful.
	NotConnected,       ///< Request failed because the client is not connected. Call connect() first.
	SendRequestFailed,  ///< Failed to send the request message.
	ConnectionClosed,   ///< Server has closed the connection.
	NoReply,            ///< No reply has arrived from the server in given timeout. In case this happens too often, you may try to increase the timeout.
	ReceiveError,       ///< There has been some other error while trying to receive a reply. Call getLastSystemError() for more info.
	InvalidReply,       ///< The reply from the server is invalid.
	SensorNotFound,     ///< Such sensor was not found.
	SensorNotMonitored, ///< Sensor is available but not monitored.
	SensorFailed,       ///< Sensor is available and monitored, but reading its value has failed.
	UnexpectedError,    ///< Internal error of this library. This should not happen unless there is a mistake in the code, please create a github issue.
};
const char * enumString( RequestStatus status ) noexcept;

/// Result and output of a sensor read request
struct SensorReadResult
{
	RequestStatus status;  ///< whether the request suceeded or why it didn't
	float sensorValue;     ///< output of a successfull request
};


//======================================================================================================================
/// HwMonitorService network client.
/** Use this to communicate with the HwMonitorService service in order to get readings from hardware sensors. */

class Client
{

 public:

	Client() noexcept;

	~Client() noexcept;

	// The connection cannot be shared.
	Client( const Client & other ) = delete;

	Client( Client && other ) noexcept = default;
	Client & operator=( Client && other ) noexcept = default;

	bool isConnected() const noexcept;

	ConnectStatus connect( const std::string & host, uint16_t port = defaultPort ) noexcept;

	bool disconnect() noexcept;

	bool setTimeout( std::chrono::milliseconds timeout ) noexcept;

	SensorReadResult requestSensorReading( const std::string & sensorID ) noexcept;

	/// Returns the system error code that caused the last failure.
	system_error_t getLastSystemError() const noexcept;

	/// Converts the numeric value of the last system error to a user-friendly string.
	std::string getLastSystemErrorStr() const noexcept;

 private:

	// a pointer so that we don't have to include the TcpSocket and all its OS dependancies here
	std::unique_ptr< own::TcpSocket > _socket;

};


//======================================================================================================================


} // namespace hwmon


#endif // HWMON_CLIENT_INCLUDED
