#ifndef OPEN_CONNECT_HARDWARE_MIGRATION_H
#define OPEN_CONNECT_HARDWARE_MIGRATION_H

#include <FS.h>

/** NVS subsystem map → LittleFS JSON once; idempotent when valid file exists. */
namespace OpenConnectHardwareMigration {

void ensure(FS* fs);

} // namespace OpenConnectHardwareMigration

#endif
