#pragma once

static const char* dda_getname(void* unused);
static void* dda_create(obs_data_t* settings, obs_source_t* source);
static void dda_destroy(void* data);