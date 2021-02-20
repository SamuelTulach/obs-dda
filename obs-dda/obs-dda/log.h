#pragma once

#define blog(log_level, format, ...)                    \
	blog(log_level, "[obs-dda] " format, ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)
#define error(format, ...) blog(LOG_ERROR, format, ##__VA_ARGS__)