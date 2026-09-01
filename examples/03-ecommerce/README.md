# E-commerce (products, users, orders)

Three related entities: catalog products, auth users, and orders with a foreign key to users.

Apply schemas **in order** — `orders` references `users`.

## Apply schemas

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

for f in schemas/products.json schemas/users.json schemas/orders.json; do
  curl -X POST "$BASE_URL/api/v1/schemas" \
    -H "Authorization: Bearer $ADMIN_TOKEN" \
    -H "Content-Type: application/json" \
    -d @"$f"
  echo
done
```

## Run examples

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>   # required for product creation
./http/orders.sh
```

## Expected behavior

| Action | Expected status |
|--------|-----------------|
| Create product without `name` | 400 (required field) |
| Register user with invalid email | 400 (`@email` validator) |
| Register user with short password | 400 (`@password` validator) |
| Create order with valid `user_id` | 201 |
| Create order with unknown `user_id` | 400 (FK constraint) |

## Next steps

- [API Reference — Foreign Keys](../../doc/api.md)
- [02-auth-users](../02-auth-users/) — user registration details
