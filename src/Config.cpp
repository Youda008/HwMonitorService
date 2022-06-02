//======================================================================================================================
// Project: HwMonitorService
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Description: configuration file loading and parsing
//======================================================================================================================

#include "Config.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#include <optional>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>
using std::optional;
using std::string;
using std::vector;

const char * const defaultConfigFileName = "settings.txt";

static const char * const refreshInterval_str = "refresh_interval";
static const char * const monitoredSensors_str = "monitored_sensors";
static const char * const port_str = "port";
static const char * const maxConnectedClients_str = "max_connected_clients";
static const char * const logLevel_str = "log_level";
static const char * const logToUDPSocket_str = "log_to_udp_socket";
static const char * const logPort_str = "log_port";

static const char * const logLevels [] =
{
	"error",
	"warning",
	"info",
	"debug",
};

static string concatLogLevels()
{
	std::ostringstream oss;
	for (auto levelStr : logLevels)
	{
		if (levelStr != logLevels[0])
			oss << '|';
		oss << levelStr;
	}
	return oss.str();
}
static const string possibleLogLevels = concatLogLevels();

static size_t indexOf( const string & logLevel )
{
	for (size_t i = 0; i < std::size(logLevels); ++i)
		if (logLevels[i] == logLevel)
			return i;
	return string::npos;
}

struct Token
{
	enum class Type
	{
		Identifier,
		Assignment,
		Bool,
		Integer,
		String,
		OpenBracket,
		CloseBracket,
		Comma,

		UnterminatedString,
		Unknown,
		End,
	};
	Type type;
	string str;
	bool boolVal;
	int intVal;
};

class Parser
{
 public:

	Parser( std::istream & stream ) : stream( stream ), currentChar( stream.get() ), line(1) {}

	Token getNextToken()
	{
		Token token;

		moveToFirstNonWhiteSpace();
		if (currentChar == EOF)
		{
			token.type = Token::Type::End;
		}
		else if (currentChar == '=')
		{
			token.type = Token::Type::Assignment;
			token.str = currentChar;
			moveToNextChar();
		}
		else if (currentChar == '[')
		{
			token.type = Token::Type::OpenBracket;
			token.str = currentChar;
			moveToNextChar();
		}
		else if (currentChar == ']')
		{
			token.type = Token::Type::CloseBracket;
			token.str = currentChar;
			moveToNextChar();
		}
		else if (currentChar == ',')
		{
			token.type = Token::Type::Comma;
			token.str = currentChar;
			moveToNextChar();
		}
		else if (isalpha(currentChar) || currentChar == '_')
		{
			token.str = readIdentifier();
			if (token.str == "true")
			{
				token.type = Token::Type::Bool;
				token.boolVal = true;
			}
			else if (token.str == "false")
			{
				token.type = Token::Type::Bool;
				token.boolVal = false;
			}
			else
			{
				token.type = Token::Type::Identifier;
			}
		}
		else if (isdigit(currentChar))
		{
			token.type = Token::Type::Integer;
			token.str = readInt();
			token.intVal = atoi( token.str.c_str() );
		}
		else if (currentChar == '"')
		{
			token.type = Token::Type::String;
			token.str = readString();
			if (currentChar == '"')
			{
				moveToNextChar();
			}
			else
			{
				token.type = Token::Type::UnterminatedString;
			}
		}
		else
		{
			token.type = Token::Type::Unknown;
			token.str = currentChar;
			moveToNextChar();
		}

		return token;
	}

	unsigned currentLine() const
	{
		return line;
	}

 private:

	void moveToNextChar()
	{
		currentChar = stream.get();
	}

	void moveToFirstNonWhiteSpace()
	{
		while (currentChar != EOF && isspace( currentChar ))
		{
			if (currentChar == '\n')
				line++;
			moveToNextChar();
		}
	}

	string readIdentifier()
	{
		std::ostringstream oss;
		oss << char( currentChar );
		moveToNextChar();
		while (currentChar != EOF && (isalpha( currentChar ) || currentChar == '_'))
		{
			oss << char( currentChar );
			moveToNextChar();
		}
		return oss.str();
	}

	string readInt()
	{
		std::ostringstream oss;
		oss << char( currentChar );
		moveToNextChar();
		while (currentChar != EOF && isdigit( currentChar ))
		{
			oss << char( currentChar );
			moveToNextChar();
		}
		return oss.str();
	}

	string readString()
	{
		std::ostringstream oss;
		moveToNextChar();
		while (currentChar != EOF && currentChar != '\n' && currentChar != '"')
		{
			oss << char( currentChar );
			moveToNextChar();
		}
		return oss.str();
	}

 private:

	std::istream & stream;
	int currentChar;
	unsigned line;

};

static string makeError( const char * format, ... )
{
	va_list args;
	va_start( args, format );

	char buffer [200];
	vsnprintf( buffer, sizeof(buffer), format, args );

	va_end( args );
	return buffer;
}

static string makeParsingError( const Parser & parser, const char * format, ... )
{
	va_list args;
	va_start( args, format );

	char finalFormat [200];
	snprintf( finalFormat, sizeof(finalFormat), "line %u: %s", parser.currentLine(), format );

	char buffer [200];
	vsnprintf( buffer, sizeof(buffer), finalFormat, args );

	va_end( args );
	return buffer;
}
		//makeError( "line %u: unknown config variable %s", token.str.c_str() ); 

static string unexpectedEOF( const Parser & parser, const char * expected )
{
	return makeParsingError( parser, "unexpected end of file, expected %s", expected );
}

static string unexpectedToken( const Parser & parser, const std::string & unexpected, const char * expected )
{
	return makeParsingError( parser, "unexpected token \"%s\", expected %s", unexpected.c_str(), expected );
}

#define EXPECT_TOKEN( tokenType, tokenName ) \
	if (token.type == Token::Type::End) \
	{\
		return unexpectedEOF( parser, tokenName ); \
	}\
	else if (token.type != Token::Type::tokenType) \
	{\
		return unexpectedToken( parser, token.str, tokenName ); \
	}

#define EXPECT_NEXT_TOKEN( tokenType, tokenName ) \
	token = parser.getNextToken(); \
	EXPECT_TOKEN( tokenType, tokenName )

static string parseList( Parser & parser, vector< string > & list )
{
	Token token;

	EXPECT_NEXT_TOKEN( OpenBracket, "[" );

	token = parser.getNextToken();
	if (token.type == Token::Type::CloseBracket)
	{
		return {};
	}

	while (true)
	{
		if (token.type == Token::Type::End)
		{
			return unexpectedEOF( parser, "string" );
		}
		else if (token.type == Token::Type::UnterminatedString)
		{
			return makeParsingError( parser, "unterminated string" );
		}
		else if (token.type != Token::Type::String)
		{
			return unexpectedToken( parser, token.str, "string" );
		}

		list.push_back( token.str );

		token = parser.getNextToken();
		if (token.type == Token::Type::CloseBracket)
		{
			break;
		}
		else if (token.type == Token::Type::Comma)
		{
			token = parser.getNextToken();
			continue;
		}
		else if (token.type == Token::Type::End)
		{
			return unexpectedEOF( parser, "\",\" or \"]\"" );
		}
		else
		{
			return unexpectedToken( parser, token.str, "\",\" or \"]\"" );
		}
	}

	return {};
}

static string parseConfig( std::istream & stream, Config & config )
{
	Parser parser( stream );
	Token token;

	bool refreshInterval_found = false;
	bool monitoredSensors_found = false;
	bool port_found = false;
	bool maxConnectedClients_found = false;
	bool logLevel_found = false;
	bool logToUDPSocket_found = false;
	bool logPort_found = false;

	while ((token = parser.getNextToken()).type != Token::Type::End)
	{
		EXPECT_TOKEN( Identifier, "variable name" );
		std::string identifier = token.str;

		EXPECT_NEXT_TOKEN( Assignment, "=" );

		if (identifier == refreshInterval_str)
		{
			EXPECT_NEXT_TOKEN( Integer, "integer" );
			config.refreshInterval_ms = token.intVal;
			refreshInterval_found = true;
		}
		else if (identifier == monitoredSensors_str)
		{
			string errorMessage = parseList( parser, config.monitoredSensors );
			if (!errorMessage.empty())
			{
				return errorMessage;
			}
			monitoredSensors_found = true;
		}
		else if (identifier == port_str)
		{
			EXPECT_NEXT_TOKEN( Integer, "integer" );
			if (token.intVal < 1 || token.intVal > UINT16_MAX)
			{
				return makeParsingError( parser, "invalid port number, possible values are 1 - %u", UINT16_MAX );
			}
			config.port = uint16_t( token.intVal );
			port_found = true;
		}
		else if (identifier == maxConnectedClients_str)
		{
			EXPECT_NEXT_TOKEN( Integer, "integer" );
			if (token.intVal < 1 || token.intVal > UINT16_MAX)
			{
				return makeParsingError( parser, "invalid %s, possible values are 1 - %u", "max_connected_clients", UINT16_MAX );
			}
			config.maxConnectedClients = uint16_t( token.intVal );
			maxConnectedClients_found = true;
		}
		else if (identifier == logLevel_str)
		{
			EXPECT_NEXT_TOKEN( Identifier, possibleLogLevels.c_str() );
			size_t levelIdx = indexOf( token.str );
			if (levelIdx == string::npos)
			{
				return makeParsingError( parser, "invalid %s, possible values are %s", "log_level", possibleLogLevels.c_str() );
			}
			config.logLevel = uint16_t( levelIdx );
			logLevel_found = true;
		}
		else if (identifier == logToUDPSocket_str)
		{
			EXPECT_NEXT_TOKEN( Bool, "boolean" );
			config.logToUDPSocket = token.boolVal;
			logToUDPSocket_found = true;
		}
		else if (identifier == logPort_str)
		{
			EXPECT_NEXT_TOKEN( Integer, "integer" );
			config.logPort = token.intVal;
			logPort_found = true;
		}
		else 
		{
			return makeParsingError( parser, "unknown configuration variable %s", identifier.c_str() );
		}
	}

	if (!refreshInterval_found)
		return makeError( "missing option %s", refreshInterval_str );
	if (!monitoredSensors_found)
		return makeError( "missing option %s", monitoredSensors_str );
	if (!port_found)
		return makeError( "missing option %s", port_str );
	if (!maxConnectedClients_found)
		return makeError( "missing option %s", maxConnectedClients_str );
	if (!logLevel_found)
		return makeError( "missing option %s", logLevel_str );
	if (!logToUDPSocket_found)
		return makeError( "missing option %s", logToUDPSocket_str );
	if (!logPort_found)
		return makeError( "missing option %s", logPort_str );

	return {};
}

static optional< string > getExecutableDir()
{
	char exePath [MAX_PATH];
	if (GetModuleFileNameA( NULL, exePath, DWORD( std::size(exePath) ) ) == 0)
	{
		return std::nullopt;
	}

	size_t lastSlashPos = 0;
	for (size_t i = 0; exePath[i] != '\0'; ++i)
	{
		if (exePath[i] == '\\')
			lastSlashPos = i;
	}
	exePath[ lastSlashPos ] = '\0';

	return string( exePath );
}

std::string readConfig( const std::string & fileName, Config & config )
{
	const optional< string > executablePath = getExecutableDir();
	if (!executablePath)
	{
		return makeError( "Failed to get executable path (error code: %d)", int( GetLastError() ) );
	}

	string filePath = *executablePath+'\\'+fileName;
	std::ifstream file( filePath.c_str(), std::ios::in );
	if (!file.is_open())
	{
		return makeError( "Failed to open config file %s (error code: %d)", filePath.c_str(), int( GetLastError() ) );
	}

	string parsingError = parseConfig( file, config );
	if (!parsingError.empty())
	{
		return makeError( "Failed to load config file %s: %s", fileName.c_str(), parsingError.c_str() );
	}

	return {};
}
