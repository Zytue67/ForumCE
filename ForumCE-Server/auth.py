from datetime import datetime, timedelta, timezone
import os

import jwt
from pwdlib import PasswordHash


password_hasher = PasswordHash.recommended()

# Development secret only.
# Before ForumCE is deployed publicly, this will come from an environment variable.
SECRET_KEY = os.environ.get(
    "FORUMCE_SECRET_KEY",
    "forumce-development-secret-change-before-production"
)

ALGORITHM = "HS256"
ACCESS_TOKEN_HOURS = 24


def hash_password(password: str) -> str:
    return password_hasher.hash(password)


def verify_password(
    password: str,
    password_hash: str
) -> bool:
    return password_hasher.verify(
        password,
        password_hash
    )


def create_access_token(
    user_id: int,
    username: str
) -> str:
    now = datetime.now(timezone.utc)

    payload = {
        "sub": str(user_id),
        "username": username,
        "iat": now,
        "exp": now + timedelta(
            hours=ACCESS_TOKEN_HOURS
        )
    }

    return jwt.encode(
        payload,
        SECRET_KEY,
        algorithm=ALGORITHM
    )


def decode_access_token(token: str):
    try:
        return jwt.decode(
            token,
            SECRET_KEY,
            algorithms=[ALGORITHM]
        )
    except jwt.InvalidTokenError:
        return None