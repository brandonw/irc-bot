#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#ifdef DEBUG
#define debug(M, ...) \
	fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif

#define log_err(M, ...) \
	fprintf(stderr, "[ERROR] %s:%d " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define log_warn(M, ...) \
	fprintf(stderr, "[WARN] %s:%d " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define log_info(M, ...) \
	fprintf(stderr, "[INFO] %s:%d " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif
