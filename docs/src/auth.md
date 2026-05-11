# Authentication

Administrative routes require **HTTP Basic** credentials checked against `mb_admin`.

End-user flows for `auth` entities use `POST /api/v1/auth/login`, which returns a JWT when `MB_JWT_SECRET`
is configured. OAuth/OIDC providers will plug into the `AuthProvider` trait in a later milestone.
