#pragma once

extern bool silvertune_entry_init(const char *plugin_path);
extern void silvertune_entry_deinit(void);
extern const void *silvertune_entry_get_factory(const char *factory_id);
