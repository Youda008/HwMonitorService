//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: main service functionality
//======================================================================================================================

#include "MyService.hpp"

#include "SvcCommon.hpp"
#include "EventLogMessages.h"

#include "Protocol.hpp"
#include "Config.hpp"

#include <CppUtils-Network/Socket.hpp>
#include <CppUtils-Essential/StringUtils.hpp>  // to_string
using namespace own;

#include "lhwm-cpp-wrapper.h"
using SensorMap = std::invoke_result_t< decltype( LHWM::GetHardwareSensorMap ) >;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <tchar.h>

#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::unique_ptr;


//======================================================================================================================
//  global

const TCHAR * const MY_SERVICE_NAME = _T("HwMonitorService");

static HANDLE g_svcStopEvent = NULL;

static std::mutex g_logMtx;
static UdpSocket g_logSocket;

static std::thread g_serverThread;
//static std::mutex g_serverMtx;
static TcpServerSocket g_serverSocket;

static Config g_config;

enum class SensorState
{
	Monitored,              ///< detected by the hardware monitoring library and monitored
	RequestedButNotFound,   ///< requested to be monitored but not detected by the hardware monitoring library
	FoundButNotMonitored,   ///< detected by the hardware monitoring library but not monitored
};
struct SensorData
{
	SensorState state;
	std::atomic< float > value;

	SensorData( SensorState state ) : state( state ), value( 0.0f ) {}
};
static unordered_map< string, SensorData > g_sensorData;


//======================================================================================================================
//  logging

enum class Severity
{
	Error,
	Warning,
	Info,
	Debug,
};
static const TCHAR * const SeverityStrings [] =
{
	_T("Error"),
	_T("Warning"),
	_T("Info"),
	_T("Debug"),
};
static void log( Severity severity, const TCHAR * format, ... )
{
	va_list args;
	va_start( args, format );

	if (unsigned( severity ) <= unsigned( g_config.logLevel ))
	{
		std::unique_lock< std::mutex > lock( g_logMtx );

		TCHAR timeStr [20];
		time_t now = time(nullptr);
		_tcsftime( timeStr, 20, _T("%Y-%m-%d %H:%M:%S"), localtime(&now) );

		TCHAR finalFormat [128];
		_sntprintf( finalFormat, 127, _T("%s [%-8s] %s\n"), timeStr, SeverityStrings[ size_t(severity) ], format );
		finalFormat[126] = _T('\n');
		finalFormat[127] = _T('\0');

		if (g_logSocket.isOpen())
		{
			TCHAR buffer [200];
			static const TCHAR tag [] = _T("[HwMonitorService] ");
			_tcscpy( buffer, tag );
			int written = _vsntprintf( buffer + _tcslen(tag), 199 - _tcslen(tag), finalFormat, args );
			buffer[ _tcslen(tag) + written - 1 ] = _T('\0');  // cut the newline

			SocketError res = g_logSocket.sendTo( { {127,0,0,1}, g_config.logPort }, buffer );
			if (res != SocketError::Success)
			{
				ReportSvcEvent( SVCEVENT_CUSTOM_ERROR,
					_T("Failed to send log message (SocketError = %hs; error code = %d)"),
					enumString( res ), int( g_logSocket.getLastSystemError() )
				);
			}
		}

		#ifdef DEBUGGING_PROCESS
			_vtprintf( finalFormat, args );
			fflush( stdout );
		#endif
	}

	va_end( args );
}

template< typename ... Args >
void reportEventAndLog( DWORD eventID, Severity severity, const TCHAR * format, Args ... args )
{
	ReportSvcEvent( eventID, format, args ... );
	#ifndef DEBUGGING_PROCESS
		log( severity, format, args ... );
	#endif
}


//======================================================================================================================
//  utils

static void populateSensorDataMap( const SensorMap & sensorMap, unordered_map< string, SensorData > & sensorData )
{
	for (const auto & sensorKV : sensorMap)
	{
		for (const auto& row : sensorKV.second)
		{
			sensorData.emplace( std::get<2>(row), SensorState::FoundButNotMonitored );
		}
	}
}


//======================================================================================================================
//  main service functionality

static void TcpServerLoop();

bool MyServiceInit()
{
	// read configuration from a file
	string loadingError = readConfig( defaultConfigFileName, g_config );
	if (!loadingError.empty())
	{
		reportEventAndLog( SVCEVENT_CUSTOM_ERROR, Severity::Error, _T("%hs"), loadingError.c_str() );
		return false;
	}

	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 1000 );

	// open logging socket
	if (g_config.logToUDPSocket)
	{
		SocketError res = g_logSocket.open();  // open at any port, we are sending, not receiving
		if (res != SocketError::Success)
		{
			ReportSvcEvent( SVCEVENT_CUSTOM_ERROR,
				_T("Failed to open logging socket (SocketError = %hs; error code = %d), detailed logging will not be available."),
				enumString( res ), int( g_logSocket.getLastSystemError() )
			);
			log( Severity::Error, _T("Failed to open logging socket, logging only to stdout.") );
		}
	}

	log( Severity::Info, _T("Initializing service") );

	// reading the hardware sensors requires admin privileges
	if (!IsUserAnAdmin())
	{
		reportEventAndLog( SVCEVENT_CUSTOM_ERROR, Severity::Error, _T("User is not admin") );
		//return false;
	}

	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 4000 );

	// read information about available sensors from the hardware monitoring library
	auto sensorInfo = LHWM::GetHardwareSensorMap();
	if (sensorInfo.empty())
	{
		reportEventAndLog( SVCEVENT_CUSTOM_ERROR, Severity::Error, _T("Failed to initalize sensor monitoring") );
		return false;
	}

	// build list of sensors that are available and that we will be monitoring
	populateSensorDataMap( sensorInfo, g_sensorData );         // fill the map with all available sensors
	for (const string & sensorID : g_config.monitoredSensors)  // update the map with sensors we will be monitoring
	{
		auto sensorDataIter = g_sensorData.find( sensorID );
		if (sensorDataIter != g_sensorData.end())
		{
			sensorDataIter->second.state = SensorState::Monitored;
		}
		else
		{
			log( Severity::Error, _T("Requested sensor %hs not found"), sensorID.c_str() );
			g_sensorData.emplace( sensorID, SensorState::RequestedButNotFound );
		}
	}

	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 1000 );

	// Create an event. The control handler function (SvcCtrlHandler)
	// signals this event when it receives the stop control code.
	g_svcStopEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if (g_svcStopEvent == NULL)
	{
		reportEventAndLog( SVCEVENT_CUSTOM_ERROR, Severity::Error, _T("Failed to create stop event") );
		return false;
	}

	// start the TCP server
	SocketError openResult = g_serverSocket.open( g_config.port );
	if (openResult == SocketError::Success)
	{
		log( Severity::Debug, _T("TCP server started") );
	}
	else
	{
		reportEventAndLog( SVCEVENT_CUSTOM_ERROR, Severity::Error,
			_T("Failed to start TCP server (SocketError = %hs; error code = %d)"),
			enumString( openResult ), int( g_serverSocket.getLastSystemError() )
		);
		CloseHandle( g_svcStopEvent );
		return false;
	}

	log( Severity::Info, _T("Service is initialized and running") );

	return true;
}

void MyServiceRun()
{
	g_serverThread = std::thread( TcpServerLoop );

	DWORD waitResult = 0;
	do
	{
		for (auto & kvPair : g_sensorData)
		{
			const string & sensorID = kvPair.first;
			auto & sensorData = kvPair.second;

			if (sensorData.state != SensorState::Monitored)
			{
				continue;
			}

			float value = LHWM::GetSensorValue( sensorID );
			if (value == 0.0)
			{
				log( Severity::Error, _T("Failed to get value from sensor %hs"), sensorID.c_str() );
			}

			sensorData.value.store( value );
		}

		// If the SvcCtrlHandler signals to stop the service, wake up and exit immediatelly.
		// If there is no signal, wait 2 seconds and repeat.
		waitResult = WaitForSingleObject( g_svcStopEvent, g_config.refreshInterval_ms );
	}
	while (waitResult == WAIT_TIMEOUT);

	log( Severity::Debug, _T("Waiting for server thread to quit") );

	g_serverThread.join();

	log( Severity::Info, _T("Service has stopped") );
}

enum class Connection
{
	Close,
	Keep,
};
static Connection serveClient( TcpSocket & clientSocket );

static void TcpServerLoop()
{
	unordered_set< ASocket * > activeSockets;
	// Keep this declared here to prevent unnecessary allocation and deallocation at every iteration.
	std::vector< ASocket * > readySockets;

	const size_t maxActiveSockets = size_t( g_config.maxConnectedClients ) + 1;
	activeSockets.insert( &g_serverSocket );

	bool keepRunning = true;
	while (keepRunning)
	{
		bool result = waitForAny( activeSockets, readySockets, std::chrono::seconds(2) );
		if (!result)  // this might be an interrupt signal, or some internal error
		{
			log( Severity::Warning, _T("select() failed (error code = %d)"), int( getLastError() ) );
			keepRunning = false;
			break;
		}

		for (ASocket * socket : readySockets)
		{
			if (TcpServerSocket * serverSocket = dynamic_cast< TcpServerSocket * >( socket ))
			{
				Endpoint from;
				TcpSocket clientSocket = serverSocket->accept( from );
				if (!clientSocket)  // means either an error or the other thread closed the socket to signal us to exit
				{
					log( Severity::Warning, _T("accept() failed (error code = %d)"), int( g_serverSocket.getLastSystemError() ) );
					keepRunning = false;
					break;
				}

				log( Severity::Debug, _T("New connection from %hs:%u at socket %u"),
					own::to_string( from.addr ).c_str(), unsigned( from.port ), unsigned( clientSocket.getSystemHandle() ) );

				activeSockets.insert( new TcpSocket( std::move(clientSocket) ) );

				// If we reached the maximum allowed number of clients, prevent further connections by removing the server socket.
				if (activeSockets.size() == maxActiveSockets)
				{
					activeSockets.erase( serverSocket );
				}
			}
			else if (TcpSocket * clientSocket = dynamic_cast< TcpSocket * >( socket ))
			{
				Connection decision = serveClient( *clientSocket );

				if (decision == Connection::Close)
				{
					activeSockets.erase( clientSocket );
					delete clientSocket;  // this also closes the socket in destructor

					// Some client slots have been freed, re-activate the server socket.
					if (activeSockets.size() - 1 < maxActiveSockets)
					{
						activeSockets.insert( &g_serverSocket );
					}
				}
			}
		}

		readySockets.clear();
	}

	for (ASocket * socket : activeSockets)
	{
		if (!dynamic_cast< TcpServerSocket * >( socket ))
		{
			delete socket;
		}
	}
}

static Connection serveClient( TcpSocket & clientSocket )
{
	const auto socketHandle = clientSocket.getSystemHandle();  // the internal system handle gets invalidated when connection breaks

	std::vector< uint8_t > requestData;
	auto recvRes = clientSocket.receiveOnce( requestData );
	if (recvRes != SocketError::Success)
	{
		if (recvRes == SocketError::ConnectionClosed)
		{
			log( Severity::Debug, _T("Connection closed at socket %u"), unsigned( socketHandle ) );
		}
		else {
			log( Severity::Debug, _T("receive() failed at socket %u (SocketError = %hs; error code = %d)"),
				unsigned( socketHandle ), enumString( recvRes ), int( clientSocket.getLastSystemError() ) );
		}
		// Timeout is not possible here because this is called on when the socket is ready.
		// Either the client disconnected or it's an error.
		return Connection::Close;
	}

	SensorRequest request;
	if (!fromBytes( requestData, request ))
	{
		log( Severity::Debug, _T("Invalid request from socket %u, disconnecting"), unsigned( socketHandle ) );
		clientSocket.send( toByteVector( SensorResponse( ResponseCode::InvalidRequest ) ) );
		// Might be an uninvited guest, let's not allow him to consume system resources.
		return Connection::Close;
	}

	auto sensorDataIter = g_sensorData.find( request.sensorID );
	if (sensorDataIter == g_sensorData.end() || sensorDataIter->second.state == SensorState::RequestedButNotFound)
	{
		log( Severity::Debug, _T("Sensor not found: %hs"), request.sensorID.c_str() );
		clientSocket.send( toByteVector( SensorResponse( ResponseCode::SensorNotFound ) ) );
		return Connection::Keep;
	}
	else if (sensorDataIter->second.state == SensorState::FoundButNotMonitored)
	{
		log( Severity::Debug, _T("Sensor not monitored: %hs"), request.sensorID.c_str() );
		clientSocket.send( toByteVector( SensorResponse( ResponseCode::SensorNotMonitored ) ) );
		return Connection::Keep;
	}

	float value = sensorDataIter->second.value.load();

	if (value <= 0.0f) // the other thread failed to retrieve temperature
	{
		log( Severity::Debug, _T("Sensor reading is not ready") );
		clientSocket.send( toByteVector( SensorResponse( ResponseCode::SensorFailed ) ) );
		return Connection::Keep;
	}

	//log( Severity::Debug, _T("Sending back value of sensor %hs: %f"), request.sensorID.c_str(), double(value) );

	SensorResponse response( ResponseCode::Success, value );

	auto sendRes = clientSocket.send( toByteVector( response ) );
	if (sendRes != SocketError::Success)
	{
		log( Severity::Warning, _T("send() failed at socket %u (SocketError = %hs; error code = %d)"),
			unsigned( socketHandle ), enumString( sendRes ), int( clientSocket.getLastSystemError() ) );
		return Connection::Close;  // client probably disconnected
	}

	return Connection::Keep;
}

void MyServiceStop()
{
	log( Severity::Info, _T("Stopping service") );

	// signal the primary thread
	SetEvent( g_svcStopEvent );

	// signal the secondary thread, this should wake up the thread from sleeping in accept
	g_serverSocket.close();
}

void MyServiceCleanup()
{
	g_serverSocket.close();

	CloseHandle( g_svcStopEvent );

	g_logSocket.close();
}
