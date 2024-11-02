#include <CppUtils-Essential/Essential.hpp>

#include <HwMonitorClient.hpp>
using namespace hwmon;

#include <CppUtils-Essential/StreamUtils.hpp>
#include <CppUtils-Essential/StringUtils.hpp>

#include <iostream>
#include <string>
using namespace std;


//======================================================================================================================

static hwmon::Client g_hwmonClient;

static bool connectToService( uint16_t port )
{
	cout << "Connecting to local port " << port << endl;
	ConnectStatus status = g_hwmonClient.connect( "127.0.0.1", port );
	if (status == ConnectStatus::Success)
	{
		cout << " -> success" << endl;
		return true;
	}
	else
	{
		cout << " -> failed: " << enumString( status )
		     << " (error code: " << g_hwmonClient.getLastSystemError() << ")" << endl;
		return false;
	}
}

static float getSensorValue( const std::string & sensorID )
{
	auto result = g_hwmonClient.requestSensorReading( sensorID );
	if (result.status != hwmon::RequestStatus::Success)
	{
		cout << "Getting temperature failed: " << enumString( result.status )
		     << " (error code: " << g_hwmonClient.getLastSystemError() << ")" << endl;
		return 0.0f;
	}
	return result.sensorValue;
}


//======================================================================================================================

int main( int /*argc*/, char * /*argv*/ [] )
{
	cout << "HwMonitoring C++ Client tester\n" << endl;

	cout << "Enter the service port: " << flush;
	uint16_t servicePort; cin >> servicePort;
	if (cin.eof())
	{
		return 0;
	}
	else if (cin.fail())
	{
		cout << "  invalid input" << endl;
		return 1;
	}

	if (!connectToService( servicePort ))
	{
		return 2;
	}

	while (true)
	{
		cout << "Enter the ID of the sensor to read: " << flush;
		string sensorID = own::read_until( cin, '\n' );
		if (cin.eof())
		{
			return 0;
		}
		else if (cin.fail())
		{
			cout << "  invalid input" << endl;
			return 1;
		}

		if (sensorID.empty())
		{
			// enter has been hit without writing anything -> behave the same as a normal terminal
			continue;
		}

		auto sensorVal = getSensorValue( sensorID );
		if (sensorVal != 0.0f)
		{
			cout << "Sensor value: " << sensorVal << endl;
		}
	}
}
