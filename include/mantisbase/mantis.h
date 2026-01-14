/**
 * @file mantis.h
 * @brief Convenience header that includes all MantisBase public API headers.
 *
 * This header provides a single include point for all MantisBase functionality.
 * It includes core components, models, utilities, and third-party dependencies.
 */

#ifndef MANTIS_H
#define MANTIS_H

// Utility functions
#include "utils/utils.h"
#include "utils/uuidv7.h"
#include "utils/soci_wrappers.h"

// Core application
#include "mantisbase.h"

// Core components
#include "core/context_store.h"
#include "core/database.h"
#include "core/exceptions.h"
#include "core/expr_evaluator.h"
#include "core/files.h"
#include "core/http.h"
#include "core/auth.h"
#include "core/logger/logger.h"
#include "core/middlewares.h"
#include "core/route_registry.h"
#include "core/router.h"
#include "core/kv_store.h"
#include "core/types.h"

// Models and data structures
#include "core/models/validators.h"
#include "core/models/entity.h"
#include "core/models/entity_schema.h"
#include "core/models/entity_schema_field.h"

// For convenience to using json,
// lets include it here
#include <nlohmann/json.hpp>
namespace mb {
    using json = nlohmann::json;
}

// For argparse lib
#include <argparse/argparse.hpp>

// Add soci include
#include <soci/soci.h>

#include <httplib.h>

#endif //MANTIS_H
