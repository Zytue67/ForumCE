"""One-time cleanup for ForumCE development data.

This script intentionally targets only the known throwaway items created during
pre-release calculator testing. It never touches the pinned Welcome forum,
Zytue67, cooler, or unrelated real users/forums.
"""
from database import create_tables, get_connection

create_tables()
c = get_connection()

# Remove the explicitly identified throwaway forums and everything beneath them.
forums = c.execute("SELECT id,name FROM forums WHERE id<>1").fetchall()
for row in forums:
    name = row["name"].strip().lower()
    if name == "test" or name.startswith("oqoqoq") or name.startswith("qqqvv"):
        print(f"Deleting development forum {row['id']}: {row['name']}")
        c.execute("DELETE FROM forums WHERE id=?", (row["id"],))

# Remove only the exact pre-release account named 'test'. Foreign-key cascades
# remove its test follows/messages/posts/replies while preserving other users.
test_user = c.execute("SELECT id,username FROM users WHERE username=? COLLATE NOCASE", ("test",)).fetchone()
if test_user:
    print(f"Deleting development account: {test_user['username']}")
    c.execute("DELETE FROM users WHERE id=?", (test_user["id"],))

c.commit()
remaining = c.execute("SELECT username FROM users ORDER BY username COLLATE NOCASE").fetchall()
print("Remaining users:", ", ".join(row["username"] for row in remaining) or "(none)")
c.close()
print("ForumCE development-data cleanup complete.")
