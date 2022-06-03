//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: definition of the network protocol used to communicate with this service
//======================================================================================================================

#ifndef PROTOCOL_INCLUDED
#define PROTOCOL_INCLUDED


#include "BinaryStream.hpp"

#include <cstdint>
#include <cstring>
#include <string>


//----------------------------------------------------------------------------------------------------------------------

enum class ResponseCode : uint32_t
{
	Success = 0,           ///< the float value from the sensor follows
	InvalidRequest = 1,    ///< the network request has invalid format
	SensorNotFound,        ///< such sensor was not found
	SensorNotMonitored,    ///< sensor is available but not monitored
	SensorFailed,          ///< sensor is available and monitored, but reading its value has failed
};

struct SensorRequest
{
	char magic [4];
	std::string sensorID;

	SensorRequest() {}
	SensorRequest( const std::string & sensorID ) : magic{'S','E','N','S'}, sensorID( sensorID ) {}

	size_t size() const
	{
		return sizeof(magic) + sensorID.size() + 1;
	}

	friend void operator<<( own::BinaryOutputStream & stream, const SensorRequest & r )
	{
		stream << r.magic;
		stream.writeString0( r.sensorID );
	}

	friend void operator>>( own::BinaryInputStream & stream, SensorRequest & r )
	{
		stream >> r.magic;
		if (strncmp( r.magic, "SENS", 4 ) != 0)
			return stream.setFailed();

		stream.readString0( r.sensorID );
	}
};

struct SensorResponse
{
	ResponseCode code;
	float value;

	SensorResponse() {}
	SensorResponse( ResponseCode code ) : code( code ), value( 0.0f ) {}
	SensorResponse( ResponseCode code, float value ) : code( code ), value( value ) {}

	static constexpr size_t size()
	{
		return sizeof(code) + sizeof(value);
	}

	friend void operator<<( own::BinaryOutputStream & stream, const SensorResponse & r )
	{
		stream.writeBigEndian( r.code );
		if (r.code == ResponseCode::Success)
			stream.writeRaw( r.value );
	}

	friend void operator>>( own::BinaryInputStream & stream, SensorResponse & r )
	{
		stream.readBigEndian( r.code );
		if (r.code == ResponseCode::Success)
			stream.readRaw( r.value );
	}
};


#endif // PROTOCOL_INCLUDED
