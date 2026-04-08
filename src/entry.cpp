#include <clap/clap.h>
#include "entry.h"

extern "C"
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

    const CLAP_EXPORT struct clap_plugin_entry clap_entry = {
        CLAP_VERSION,
        silvertune_entry_init,
        silvertune_entry_deinit,
        silvertune_entry_get_factory
    };

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
