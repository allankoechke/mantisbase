# Foreign Key Handling in Schema API Operations

## Schema Creation (POST /api/v1/schemas)

### Flow:
1. **Request Parsing**: JSON body is parsed and converted to `EntitySchema` via `EntitySchema::fromSchema(body)`
2. **Validation**: `eSchema.validate()` is called, which:
   - Validates foreign key references (checks if referenced tables exist)
   - Validates referenced columns exist in target tables
   - Validates type compatibility
3. **Table Creation**: `EntitySchema::createTable(eSchema)` is called:
   - Generates DDL with `toDDL()` which includes FOREIGN KEY constraints
   - Creates the table in a transaction
   - Saves schema to `mb_tables`
   - Adds routes to router cache

### Foreign Key Handling: ✅ **WORKING**
- Foreign keys are included in the DDL generation
- All constraints (ON UPDATE, ON DELETE) are properly generated
- Validation ensures referenced tables/columns exist

### Example:
```json
POST /api/v1/schemas
{
  "name": "orders",
  "type": "base",
  "fields": [
    {
      "name": "user_id",
      "type": "string",
      "required": true,
      "foreign_key": {
        "table": "users",
        "column": "id",
        "on_update": "CASCADE",
        "on_delete": "RESTRICT"
      }
    }
  ]
}
```

This creates a table with the foreign key constraint properly included.

---

## Schema Updates (PATCH /api/v1/schemas/:schema_name_or_id)

### Flow:
1. **Load Old Schema**: Retrieves existing schema from `mb_tables`
2. **Merge Changes**: Calls `new_entity.updateWith(new_schema)` to merge updates
3. **Validation**: Validates the updated schema (including foreign key references)
4. **Field Updates**: For each field in the update:
   - **Existing Fields** (matched by `id`):
     - Checks for changes in: name, required, primary_key, unique, default_value
     - Applies ALTER TABLE statements for each change
   - **New Fields**:
     - Creates column with `add_column()`
     - Adds constraints (PRIMARY KEY, NOT NULL, UNIQUE, DEFAULT)

### Foreign Key Handling: ⚠️ **PARTIALLY WORKING**

#### Current Behavior:
- ✅ Foreign key properties are updated in the schema JSON stored in `mb_tables`
- ✅ Validation checks foreign key references
- ❌ **Foreign key constraints are NOT added/modified/removed in the database**

#### What's Missing:
When updating a field's foreign key properties, the code does NOT:
1. Add foreign key constraint when a field gets a foreign key
2. Modify foreign key constraint when foreign key properties change (table, column, policies)
3. Remove foreign key constraint when foreign key is removed
4. Add foreign key constraint when a new field with foreign key is created

#### Example Issues:

**Scenario 1: Adding Foreign Key to Existing Field**
```json
PATCH /api/v1/schemas/orders
{
  "fields": [
    {
      "id": "mbf_12345",
      "name": "user_id",
      "foreign_key": {
        "table": "users",
        "column": "id",
        "on_update": "CASCADE",
        "on_delete": "RESTRICT"
      }
    }
  ]
}
```
**Result**: Schema JSON is updated, but database constraint is NOT added.

**Scenario 2: Creating New Field with Foreign Key**
```json
PATCH /api/v1/schemas/orders
{
  "fields": [
    {
      "name": "product_id",
      "type": "string",
      "foreign_key": {
        "table": "products",
        "column": "id"
      }
    }
  ]
}
```
**Result**: Column is created, but foreign key constraint is NOT added.

**Scenario 3: Removing Foreign Key**
```json
PATCH /api/v1/schemas/orders
{
  "fields": [
    {
      "id": "mbf_12345",
      "name": "user_id",
      "foreign_key": null
    }
  ]
}
```
**Result**: Schema JSON is updated, but database constraint is NOT removed.

---

## What Needs to Be Fixed

The `EntitySchema::updateTable()` method in `src/core/models/entity_schema_crud.cpp` needs to handle foreign key changes:

1. **When updating existing fields** (around line 198-364):
   - Add check: `if (old_field.isForeignKey() != new_field.isForeignKey() || foreign_key_changed)`
   - If foreign key was added: `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY ...`
   - If foreign key was removed: `ALTER TABLE ... DROP CONSTRAINT ...`
   - If foreign key properties changed: Drop old constraint, add new one

2. **When creating new fields** (around line 367-382):
   - After creating the column, check if field has foreign key
   - If yes: `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY ...`

3. **Database-specific considerations**:
   - **SQLite3**: Limited ALTER TABLE support - may need table recreation
   - **PostgreSQL**: Full support for ADD/DROP FOREIGN KEY constraints
   - **MySQL**: Full support for ADD/DROP FOREIGN KEY constraints

---

## Current Workaround

To add/modify foreign key constraints after schema creation:
1. Manually execute SQL ALTER TABLE statements
2. Or drop and recreate the table (data loss risk)

---

## Summary

| Operation | Foreign Key Support | Status |
|-----------|-------------------|--------|
| Schema Creation | Full support | ✅ Working |
| Schema Update - Add FK | Not implemented | ❌ Missing |
| Schema Update - Modify FK | Not implemented | ❌ Missing |
| Schema Update - Remove FK | Not implemented | ❌ Missing |
| Schema Update - New Field with FK | Not implemented | ❌ Missing |

