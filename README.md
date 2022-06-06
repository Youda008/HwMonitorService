# HwMonitorService

A Windows service that encapsulates LibreHardwareMonitorLib, which is a part of [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)
and provides its sensor readings via a TCP socket on localhost.


## Why would i do that?

There are few advantages running such sensor readings as a service, over doing it directly in your own application.
1. Service starts automatically when Windows starts, even before any user logs in.
2. Service runs silently in the background, no window is cluttering your work space.
3. Reading of hardware sensors requires administrator privileges. If you do it directly in your application, you will have to confirm UAC dialog everytime it starts.
   Service always runs as administrator, so no confirmation is needed.
4. It is generally not a good idea to read hardware sensors too often or from many applications at once.
   Reading too often can produce incorrect results and accessing the hardware from more applications at the same time can bug the device.
   This service reads the sensors regularly once per configured time period, and then gives its clients the last read value.
5. You can read the sensor values from any programming language without the need of any inter-language bindings. All you need is a socket API.
   

## How to install

1. Get the binaries. Either download them from the [Releases](https://github.com/Youda008/HwMonitorService/releases) page or go to section #how-to-build.
2. Create a directory in `Program Files` and extract the files into it.
3. Run `ListLHWMSensors.exe`, it will show you what hardware sensors are available on your computer.
4. Open `settings.txt` and insert all the sensors you want to monitor into the field `monitored_sensors`.
   A sensor is always identified by the last string of each sensor entry in the output of `ListLHWMSensors.exe`, for example `"/amdcpu/0/temperature/2"`.
   Edit the other settings, if you want.<br/>
   NOTE: To edit a file in Program Files you either need to run the Notepad as administrator or add yourself write permissions in the "Security" tab of file properties.
4. Run the `CreateService.bat` as administrator, it will register the executable as a Windows service and start it.
5. Verify that the service is running via the "Services" tab in the Task Manager.


## How to use

### Reading sensor values

The service reads the sensors listed in [settings.txt](deploy-package/settings.txt) once per a configured time period and then publishes its last read value via a TCP server running at configured port.

To read the sensor value you need to:
1. create a TCP socket and connect to `127.0.0.1:port`
2. send a request in the following format
```
   +---------+------------------+----+
   | S E N S | sensor ID string | \0 |
   +---------+------------------+----+
```
   The first 4 bytes are ASCII encoded characters "SENS".
   Then there is a null-terminated string of sensor ID - the one you entered in [settings.txt](deploy-package/settings.txt).

3. wait for a response that will have one of the two formats depending on status code
   format in case status code = 0
```
   +-----------------+-----------------+
   | (4) status code | (4) float value |
   +-----------------+-----------------+
```
   format in case status code > 0
```
   +-----------------+
   | (4) status code |
   +-----------------+
```
   First 4 bytes are a big endian unsigned integer, whose value have this meaning<br/>
   0 - success, the 4 byte float value from the sensor follows<br/>
   1 - the network request has invalid format<br/>
   2 - sensor with this ID was not found<br/>
   3 - sensor is available but not monitored (not entered in `[settings.txt](deploy-package/settings.txt)`)<br/>
   4 - sensor is available and monitored, but reading its value has failed<br/>
   
The primary source of truth about the protocol is [Protocol.hpp](src/Protocol.hpp).

If you are writing a C++ application, you can avoid fiddling with system sockets or importing a networking library and implementing the protocol
by compiling and linking a minimalistic client library in directory `tools/CppClient`.
The CppClient does not have a Visual Studio project but a CMakeLists, because it produces a static library and that needs to be compiled
with the same compiler (even same version and same settings) as you are using for your own project. See instructions in [tools/CppClient/README](tools/CppClient/README.md).


### Troubleshooting

If the service doesn't work correctly or refuses to start, you can diagnose the problems using the "LogReader" tool that is part of this project.
The HwMonitorService logs its state and errors to an UDP socket whose port is configured in [settings.txt](deploy-package/settings.txt) in field `log_port`.
One packet corresponds to one log message. The "LogReader" tool will listen on that port and print everything that arrives to it.

Start the "LogReader.exe" first, then go Task Manager and restart the service. You should see what errors the service encountered.
If you change the `log_port` in [settings.txt](deploy-package/settings.txt), then add it as a command line parameter to LogReader.exe, for example
`LogReader.exe 12345`.

If nothing is showing at all, alternative is to right-click on This Computer and select Manage. Then go to Event Viewer -> Windows Logs -> Application.
HwMonitorService should have its own entries there explaining what went wrong.


## How to build

Getting this to build is slightly difficult, because the build of the [lhwm-wrapper](https://gitlab.com/OpenRGBDevelopers/lhwm-wrapper/) (not part of this project) is not done very well.

First you need to initialize all submodules.
```
git submodule update --init
```

Then, either you need to open "external\lhwm-wrapper\lhwm-wrapper\lhwm-wrapper.csproj" and change the output file name from "LhwmBindingsLib.dll" to "lhwm-wrapper.dll",
or open "external\lhwm-wrapper\lhwm-cpp-wrapper\lhwm-cpp-wrapper\lhwm-cpp-wrapper.cpp" and change the `#using "lhwm-wrapper.dll"` to `#using "LhwmBindingsLib.dll"`.

You also need to add the output directory of the "lhwm-wrapper.csproj" to the additional library directories of the "lhwm-cpp-wrapper.vcxproj".

And finally, output directories of both the "lhwm-wrapper.csproj" and "lhwm-cpp-wrapper.vcxproj" need to be included in the additional library directories of the "HwMonitorService.vcxproj".

Some other manual fixes may be needed if the author of [lhwm-wrapper](https://gitlab.com/OpenRGBDevelopers/lhwm-wrapper/) decides to change his build system.

When everytime is setup to link with other components correctly, you should be able to just open "HwMonitorService.sln" in Visual Studio and build everything.