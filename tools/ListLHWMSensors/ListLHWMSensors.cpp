#include "lhwm-cpp-wrapper.h"

#include <cstdio>

int main( int argc, char * argv [] )
{
	/* sensorMap =
	{
		"name" :
		{
			{ "a", "b", "c" },
			...
		},
		...
	}
	*/
	const auto sensorMap = LHWM::GetHardwareSensorMap();

	puts("{\n");
	for (const auto & sensorKV : sensorMap)
	{
		printf("\t\"%s\" = \n", sensorKV.first.c_str());
		printf("\t{\n");
		for (const auto & row : sensorKV.second)
		{
			printf("\t\t{ \"%s\", \"%s\", \"%s\" },\n", std::get<0>(row).c_str(), std::get<1>(row).c_str(), std::get<2>(row).c_str());
		}
		printf("\t},\n");
	}
	puts("}\n");

	printf("Press enter to exit\n");
	while (getchar() != '\n') {}

	return 0;
}
