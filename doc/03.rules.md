@page rest_api_rules API Access Rules

MantisBase uses a mode-based access control system with optional JavaScript expression evaluation for fine-grained permissions. Each entity schema defines five access rules that control who can perform CRUD operations.

---

## Overview

Access rules in MantisBase use a combination of **modes** and **expressions** to control endpoint access. Rules are defined per entity and evaluated automatically by the `hasAccess()` middleware on all entity endpoints.

---

## Rule Types

Every entity supports five access rule types, one for each CRUD operation:

| Rule Type | HTTP Method | Endpoint                              | Purpose                    |
|-----------|-------------|---------------------------------------|----------------------------|
| `list`    | GET         | `/api/v1/entities/<entity>`           | Controls who can list records |
| `get`     | GET         | `/api/v1/entities/<entity>/:id`       | Controls who can view a record |
| `add`     | POST        | `/api/v1/entities/<entity>`           | Controls who can create records |
| `update`  | PATCH       | `/api/v1/entities/<entity>/:id`       | Controls who can update records |
| `delete`  | DELETE      | `/api/v1/entities/<entity>/:id`       | Controls who can delete records |

Each rule has two properties:
- **`mode`** - Determines the access control strategy
- **`expr`** - JavaScript expression (only evaluated when `mode` is `"custom"`)

---

## Access Modes

### Empty Mode (Admin Only)

When `mode` is empty or not set, the endpoint requires admin authentication:

```json
{
  "mode": "",
  "expr": ""
}
```

**Behavior:** Only users authenticated as admins (`mb_admins` entity) can access the endpoint.

### Public Mode

When `mode` is `"public"`, the endpoint is open to everyone:

```json
{
  "mode": "public",
  "expr": ""
}
```

**Behavior:** No authentication required. Anyone can access the endpoint.

### Auth Mode

When `mode` is `"auth"`, any valid authentication is accepted:

```json
{
  "mode": "auth",
  "expr": ""
}
```

**Behavior:** User must be authenticated (from any entity table), but no additional checks are performed.

### Custom Mode

When `mode` is `"custom"`, the `expr` JavaScript expression is evaluated:

```json
{
  "mode": "custom",
  "expr": "auth.id != \"\" && auth.user.verified == true"
}
```

**Behavior:** Expression is evaluated using Duktape (JavaScript engine). If expression returns `true`, access is granted.

If you want to tie authenticated users to specific entity, use expression such as; 

```
auth.entity == 'your entity name'
```

---

## Expression Evaluation

When using `mode: "custom"`, expressions are evaluated using **Duktape**, a lightweight JavaScript engine. Expressions have access to two main context objects:

### Authentication Context (`auth`)

Contains information about the authenticated user:

```javascript
auth.type      // "guest", "user", or "admin"
auth.id        // User's unique identifier (null for guests)
auth.entity    // Entity table user authenticated against (e.g., "users", "mb_admins")
auth.token     // JWT token string (null for guests)
auth.user      // Full user record from database (null if not found or guest)
```

**Example:**
```javascript
// Check if user is authenticated
auth.id != ""

// Check if user is admin
auth.entity == "mb_admins"

// Access user fields
auth.user.email
auth.user.verified
auth.user.role
```

### Request Context (`req`)

Contains data from the current HTTP request:

```javascript
req.remoteAddr    // Client IP address
req.remotePort    // Client port
req.localAddr     // Server IP address
req.localPort     // Server port
req.body          // Request body as JSON object
```

**Example:**
```javascript
// Access request body fields
req.body.title
req.body.user_id
req.body.status
```

---

## Rule Examples

### Public Access

Make an endpoint accessible to everyone:

```json
{
  "list": {"mode": "public", "expr": ""},
  "get": {"mode": "public", "expr": ""}
}
```

### Admin Only

Restrict access to admins only:

```json
{
  "delete": {"mode": "", "expr": ""}
}
```

### Authenticated Users Only

Allow any authenticated user:

```json
{
  "add": {"mode": "auth", "expr": ""},
  "update": {"mode": "auth", "expr": ""}
}
```

### Ownership-Based Rules

Allow users to access only their own records:

```json
{
  "get": {"mode": "custom", "expr": "auth.id == req.body.user_id"},
  "update": {"mode": "custom", "expr": "auth.id == req.body.author_id"}
}
```

### Role-Based Rules

Check user properties from the database:

```json
{
  "list": {
    "mode": "custom",
    "expr": "auth.user.verified == true && auth.user.role == 'premium'"
  }
}
```

### Complex Conditional Rules

Combine multiple conditions:

```json
{
  "update": {
    "mode": "custom",
    "expr": "auth.entity == 'mb_admins' || (auth.id == req.body.author_id && req.body.status != 'published')"
  },
  "delete": {
    "mode": "custom",
    "expr": "auth.entity == 'mb_admins'"
  }
}
```

---

## Setting Rules via API

### During Schema Creation

```json
{
  "name": "posts",
  "type": "base",
  "fields": [
    {"name": "title", "type": "string", "required": true},
    {"name": "content", "type": "string"},
    {"name": "author_id", "type": "string", "required": true}
  ],
  "rules": {
    "list": {"mode": "public", "expr": ""},
    "get": {"mode": "public", "expr": ""},
    "add": {"mode": "auth", "expr": ""},
    "update": {"mode": "custom", "expr": "auth.id == req.body.author_id || auth.entity == 'mb_admins'"},
    "delete": {"mode": "", "expr": ""}
  }
}
```

### Updating Existing Rules

```json
PATCH /api/v1/schemas/posts

{
  "rules": {
    "list": {"mode": "auth", "expr": ""},
    "get": {"mode": "auth", "expr": ""},
    "update": {"mode": "custom", "expr": "auth.user.verified == true"}
  }
}
```

---

## Expression Limitations

Since expressions are evaluated using Duktape (a lightweight JavaScript engine), there are some limitations:

- **No async operations** - Expressions must be synchronous
- **Limited standard library** - Not all JavaScript features are available
- **Isolated context** - Each evaluation runs in a fresh context
- **Boolean result** - Expression must evaluate to a boolean value

### Supported Operations

- **Comparison**: `==`, `!=`, `===`, `!==`, `<`, `<=`, `>`, `>=`
- **Logical**: `&&` (AND), `||` (OR), `!` (NOT)
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Grouping**: `()` for precedence
- **Property access**: `auth.user.email`, `req.body.title`

### Common Patterns

```javascript
// Check authentication
auth.id != "" && auth.id != null

// Check admin
auth.entity == "mb_admins"

// Check user property
auth.user && auth.user.verified == true

// Check request body
req.body && req.body.user_id == auth.id

// Complex conditions
(auth.entity == "mb_admins") || (auth.id == req.body.owner_id && auth.user.active == true)
```

---

## Default Behavior

### System Tables

System tables (like `mb_tables`, `mb_admins`) always require admin authentication regardless of configured rules.

### Empty Expressions

When `mode` is `"custom"` but `expr` is empty, the expression evaluates to `false`, denying access. But always admins will still have access despite this.

---

## Best Practices

### 1. Start Restrictive

Begin with strict rules and gradually relax them:

```json
// Start with admin-only
{"mode": "", "expr": ""}

// Then allow authenticated users
{"mode": "auth", "expr": ""}

// Finally, add custom conditions
{"mode": "custom", "expr": "auth.user.verified == true"}
```

### 2. Use Consistent Field Names

Establish consistent naming for ownership fields across your entities:

```javascript
// Good: consistent naming
auth.id == req.body.user_id
auth.id == req.body.owner_id
auth.id == req.body.created_by

// Avoid: inconsistent naming
```

### 3. Validate User Data Exists

Always check if user data exists before accessing properties:

```javascript
// Good: check existence first
auth.user && auth.user.verified == true

// Bad: may throw error if user is null
auth.user.verified == true
```

### 4. Test Edge Cases

Test your rules with:
- Unauthenticated users (guests)
- Authenticated users from different entities
- Admin users
- Users with missing or null properties

---

## Troubleshooting

### Access Denied Errors

1. **Check authentication**: Verify the user is properly authenticated
2. **Check mode**: Ensure the mode matches your intended behavior
3. **Check expression**: For custom mode, verify the expression syntax
4. **Check user data**: Ensure `auth.user` exists and has expected properties
5. **Check logs**: Look for expression evaluation errors in server logs

### Expression Evaluation Errors

If an expression fails to evaluate:
- Check for syntax errors (missing quotes, parentheses, etc.)
- Verify all referenced properties exist
- Ensure boolean result (expressions must return true/false)
- Check server logs for detailed error messages

### Common Mistakes

```javascript
// Wrong: comparing with == instead of !=
auth.id == ""  // This checks if ID is empty (usually wrong)

// Right: check if ID exists
auth.id != "" && auth.id != null

// Wrong: accessing nested property without checking parent
auth.user.email  // May fail if auth.user is null

// Right: check parent first
auth.user && auth.user.email
```

---

## Summary

MantisBase access rules provide flexible, mode-based access control with optional JavaScript expression evaluation. Use modes for simple cases (`public`, `auth`, empty for admin) and custom expressions for complex, fine-grained permissions.
