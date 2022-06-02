//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: configuration file loading and parsing
//======================================================================================================================

#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED

#include <tchar.h>

#include <cstdint>
#include <string>
#include <vector>

extern const char * const defaultConfigFileName;

struct Config
{
	unsigned refreshInterval_ms;
	std::vector< std::string > monitoredSensors;
	uint16_t port;
	uint16_t maxConnectedClients;
	unsigned logLevel;
	bool logToUDPSocket;
	uint16_t logPort;
};

/*enum class ConfigResult
{
	Success,
	CannotGetExePath,
	CannotOpenFile,
	CannotRead,
	InvalidFormat,
	MissingOptions,
};*/

std::string readConfig( const std::string & fileName, Config & config );

#endif // CONFIG_INCLUDED
