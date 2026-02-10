/* Minimal external plugin ABI header for GridLAB-D
   Generated via configure_file() into build headers as config_external.h.
   External plugins include platform.h -> config_external.h (this file).

   We forward to the generated internal config.h, but keep the external name.
*/
#ifndef GLD_CONFIG_EXTERNAL_H
#define GLD_CONFIG_EXTERNAL_H

/* Pull in generated internal configuration macros */
#include "config.h"

/* Identify external/plugin builds (if any part of the code tests this) */
#ifndef GLDPLUGIN
#define GLDPLUGIN 1
#endif

#endif /* GLD_CONFIG_EXTERNAL_H */