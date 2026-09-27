from database import get_connection, create_tables

create_tables()
c=get_connection()
rows=c.execute("SELECT id,name FROM forums WHERE id<>1").fetchall()
for row in rows:
    n=row["name"].strip().lower()
    if n == "test" or n.startswith("oqoqoq") or n.startswith("qqqvv"):
        print(f"Deleting test forum {row['id']}: {row['name']}")
        c.execute("DELETE FROM forums WHERE id=?",(row["id"],))
c.commit(); c.close()
print("Test-forum cleanup complete.")
