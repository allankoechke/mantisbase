#!/usr/bin/env bash
# Dump current schemas and restore from backup file.
set -euo pipefail

DUMP_FILE="${1:-./schema-backup.json}"
MB="${MB_BIN:-./mantisbase}"

echo "=== Dump schemas to $DUMP_FILE ==="
"$MB" migrate schema --to "$DUMP_FILE"
echo "Wrote $DUMP_FILE"

echo "=== Restore from $DUMP_FILE ==="
"$MB" migrate schema --from "$DUMP_FILE"
echo "Restore complete."
