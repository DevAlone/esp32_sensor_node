#pragma once

#include <string>

/// returns sensors data as json list,
/// for example:
/// [
/// 	{
/// 		"type": "dht11",
/// 		"pin": 9,
/// 		"timestamp_ms": 11134612,
/// 		"value": 10.7
/// 	},
/// ]
std::string getSensorsDataJSON();
