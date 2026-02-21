#pragma once

#define source_warn(format, ...) blog(LOG_WARNING, "obs_register_source: " format, ##__VA_ARGS__)
#define output_warn(format, ...) blog(LOG_WARNING, "obs_register_output: " format, ##__VA_ARGS__)
#define encoder_warn(format, ...) blog(LOG_WARNING, "obs_register_encoder: " format, ##__VA_ARGS__)
#define service_warn(format, ...) blog(LOG_WARNING, "obs_register_service: " format, ##__VA_ARGS__)