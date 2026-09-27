import os
from pathlib import Path
import sqlite3

DATABASE_PATH = Path(
    os.environ.get(
        "FORUMCE_DB_PATH",
        str(Path(__file__).parent / "forumce.db")
    )
)

FORUMS = [
    (1, "Welcome to ForumCE"),
]


def get_connection():
    connection = sqlite3.connect(DATABASE_PATH, timeout=10)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA journal_mode = WAL")
    return connection


def create_tables():
    c = get_connection()
    c.executescript("""
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT NOT NULL UNIQUE COLLATE NOCASE,
        password_hash TEXT NOT NULL,
        bio TEXT NOT NULL DEFAULT '',
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS follows (
        follower_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        following_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (follower_id, following_id),
        CHECK (follower_id != following_id)
    );

    CREATE TABLE IF NOT EXISTS forums (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
        updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS threads (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        forum_id INTEGER NOT NULL REFERENCES forums(id) ON DELETE CASCADE,
        author_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        title TEXT NOT NULL,
        body TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS posts (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        thread_id INTEGER NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
        author_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        body TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS replies (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        post_id INTEGER NOT NULL REFERENCES posts(id) ON DELETE CASCADE,
        author_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        body TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS post_likes (
        post_id INTEGER NOT NULL REFERENCES posts(id) ON DELETE CASCADE,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (post_id, user_id)
    );

    CREATE TABLE IF NOT EXISTS reply_likes (
        reply_id INTEGER NOT NULL REFERENCES replies(id) ON DELETE CASCADE,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (reply_id, user_id)
    );

    CREATE TABLE IF NOT EXISTS conversations (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user1_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        user2_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CHECK (user1_id < user2_id),
        UNIQUE (user1_id, user2_id)
    );

    CREATE TABLE IF NOT EXISTS messages (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
        sender_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        body TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS conversation_state (
        conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        last_read_message_id INTEGER,
        hidden INTEGER NOT NULL DEFAULT 0,
        PRIMARY KEY (conversation_id, user_id)
    );

    CREATE TABLE IF NOT EXISTS notifications (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        actor_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
        type TEXT NOT NULL,
        source_id INTEGER,
        text TEXT NOT NULL DEFAULT '',
        read INTEGER NOT NULL DEFAULT 0,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS notification_preferences (
        user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
        master INTEGER NOT NULL DEFAULT 1,
        messages INTEGER NOT NULL DEFAULT 1,
        replies INTEGER NOT NULL DEFAULT 1,
        forum_activity INTEGER NOT NULL DEFAULT 1,
        new_followers INTEGER NOT NULL DEFAULT 1
    );

    CREATE INDEX IF NOT EXISTS idx_threads_forum ON threads(forum_id, id DESC);
    CREATE INDEX IF NOT EXISTS idx_posts_thread ON posts(thread_id, id ASC);
    CREATE INDEX IF NOT EXISTS idx_replies_post ON replies(post_id, id ASC);
    CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id DESC);
    CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications(user_id, read, id DESC);
    """)

    # Upgrade older ForumCE databases in place.
    forum_columns = {
        row[1]
        for row in c.execute("PRAGMA table_info(forums)").fetchall()
    }

    if "created_by" not in forum_columns:
        c.execute(
            "ALTER TABLE forums ADD COLUMN created_by "
            "INTEGER REFERENCES users(id) ON DELETE SET NULL"
        )

    if "updated_at" not in forum_columns:
        c.execute("ALTER TABLE forums ADD COLUMN updated_at TEXT")
        c.execute("""
            UPDATE forums
            SET updated_at = COALESCE((
                SELECT MAX(x.created_at)
                FROM (
                    SELECT t.created_at created_at
                    FROM threads t
                    WHERE t.forum_id = forums.id

                    UNION ALL

                    SELECT p.created_at
                    FROM posts p
                    JOIN threads t ON t.id = p.thread_id
                    WHERE t.forum_id = forums.id

                    UNION ALL

                    SELECT r.created_at
                    FROM replies r
                    JOIN posts p ON p.id = r.post_id
                    JOIN threads t ON t.id = p.thread_id
                    WHERE t.forum_id = forums.id
                ) x
            ), STRFTIME('%Y-%m-%d %H:%M:%f', 'now'))
        """)

    c.executemany(
        "INSERT OR IGNORE INTO forums(id, name) VALUES (?, ?)",
        FORUMS
    )

    c.execute("""
        INSERT OR IGNORE INTO notification_preferences(user_id)
        SELECT id FROM users
    """)

    c.commit()
    c.close()


if __name__ == "__main__":
    create_tables()
    print("ForumCE database upgraded successfully.")