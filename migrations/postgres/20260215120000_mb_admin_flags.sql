-- Admin account flags (active, password reset gate). Safe on existing DBs.
ALTER TABLE mb_admin ADD COLUMN IF NOT EXISTS active BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE mb_admin ADD COLUMN IF NOT EXISTS password_reset_required BOOLEAN NOT NULL DEFAULT FALSE;
