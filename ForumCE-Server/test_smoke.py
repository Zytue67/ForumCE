import uuid, sys, types, tempfile
from pathlib import Path
from fastapi.testclient import TestClient
# Test harness stub: production auth.py needs pwdlib, unavailable in this isolated runner.
auth = types.ModuleType("auth")
auth.hash_password=lambda p: "h:"+p
auth.verify_password=lambda p,h: h=="h:"+p
auth.create_access_token=lambda uid,username: f"{uid}:{username}"
def decode(t):
    try: uid,name=t.split(":",1); return {"sub":uid,"username":name}
    except: return None
auth.decode_access_token=decode
sys.modules["auth"]=auth
import database
database.DATABASE_PATH = Path(tempfile.mkdtemp()) / "test_forumce.db"
from main import app

client = TestClient(app)

def test_core_flow():
    suffix = uuid.uuid4().hex[:8]
    a = f"test_{suffix}a"
    b = f"test_{suffix}b"
    pw = "testpass123"
    assert client.get("/").status_code == 200
    assert client.post("/register", json={"username":a,"password":pw}).status_code == 201
    assert client.post("/register", json={"username":b,"password":pw}).status_code == 201
    la = client.post("/login", json={"username":a,"password":pw}); assert la.status_code == 200
    lb = client.post("/login", json={"username":b,"password":pw}); assert lb.status_code == 200
    ha={"Authorization":f"Bearer {la.json()['access_token']}"}; hb={"Authorization":f"Bearer {lb.json()['access_token']}"}
    assert client.get("/me",headers=ha).json()["username"] == a
    assert client.put("/me/bio",headers=ha,json={"bio":"ForumCE smoke test"}).status_code == 200
    assert client.get(f"/users/search?q={b}",headers=ha).json()["users"][0]["username"] == b
    assert client.post(f"/users/{b}/follow",headers=ha).status_code == 200
    t=client.post("/forums/1/threads",headers=ha,json={"title":"Smoke test","body":"Root body"}); assert t.status_code == 201
    pid=t.json()["root_post_id"]
    assert client.post(f"/posts/{pid}/replies",headers=hb,json={"body":"Reply"}).status_code == 201
    assert client.post(f"/posts/{pid}/like",headers=hb).json()["liked"] is True
    assert client.post(f"/messages/{b}",headers=ha,json={"body":"Hello"}).status_code == 201
    assert client.get("/messages",headers=hb).json()["conversations"][0]["unread_count"] >= 1
    assert client.get("/notifications",headers=ha).status_code == 200

    # Calculator write-path persistence: create forum, post, reply, and like,
    # then fetch each resource again as a fresh read.
    fname = f"Persist_{suffix}"
    fr = client.post("/forums", headers=ha, json={"name": fname})
    assert fr.status_code == 201
    fid = fr.json()["id"]; tid = fr.json()["thread_id"]
    assert any(x["id"] == fid for x in client.get("/forums").json()["forums"])
    post = client.post(f"/threads/{tid}/posts", headers=ha, json={"body":"Persistent calculator post"})
    assert post.status_code == 201
    post_id = post.json()["id"]
    posts = client.get(f"/threads/{tid}/posts?page=1&limit=10", headers=ha).json()["posts"]
    assert any(x["id"] == post_id and x["username"] == a for x in posts)
    rep = client.post(f"/posts/{post_id}/replies", headers=ha, json={"body":"Persistent calculator reply"})
    assert rep.status_code == 201
    replies = client.get(f"/posts/{post_id}/replies?page=1&limit=10", headers=ha).json()["replies"]
    assert any(x["id"] == rep.json()["id"] and x["username"] == a for x in replies)
    liked = client.post(f"/posts/{post_id}/like", headers=ha).json()
    assert liked["liked"] is True
    posts2 = client.get(f"/threads/{tid}/posts?page=1&limit=10", headers=ha).json()["posts"]
    assert next(x for x in posts2 if x["id"] == post_id)["liked"] == 1

def test_cors_localhost():
    r=client.options("/me",headers={"Origin":"http://localhost:5500","Access-Control-Request-Method":"GET"})
    assert r.status_code == 200
    assert r.headers.get("access-control-allow-origin") == "http://localhost:5500"


def test_forum_activity_sort_and_delete():
    suffix = uuid.uuid4().hex[:8]
    creator = "Zytue67"
    pw = "testpass123"
    # This isolated database has no real creator account, so make one.
    r = client.post("/register", json={"username":creator,"password":pw})
    assert r.status_code in (201,409)
    login = client.post("/login", json={"username":creator,"password":pw})
    assert login.status_code == 200
    h={"Authorization":f"Bearer {login.json()['access_token']}"}

    fa=client.post("/forums",headers=h,json={"name":f"Old_{suffix}"}); assert fa.status_code==201
    fb=client.post("/forums",headers=h,json={"name":f"New_{suffix}"}); assert fb.status_code==201
    # Create a reply in the older forum last; it must bump above the other forum.
    pa=client.post(f"/threads/{fa.json()['thread_id']}/posts",headers=h,json={"body":"root"}); assert pa.status_code==201
    rr=client.post(f"/posts/{pa.json()['id']}/replies",headers=h,json={"body":"bump"}); assert rr.status_code==201
    forums=client.get("/forums").json()["forums"]
    assert forums[0]["id"] == 1
    ids=[x["id"] for x in forums]
    assert ids.index(fa.json()["id"]) < ids.index(fb.json()["id"])

    assert client.delete(f"/forums/{fb.json()['id']}",headers=h).status_code == 200
    assert all(x["id"] != fb.json()["id"] for x in client.get("/forums").json()["forums"])
    assert client.delete("/forums/1",headers=h).status_code == 403

def test_social_calculator_flow():
    suffix = uuid.uuid4().hex[:8]
    pw = "testpass123"
    # Ensure the special creator exists in the isolated database.
    client.post("/register", json={"username":"Zytue67","password":pw})
    a=f"alpha_{suffix}"; b=f"beta_{suffix}"; z=f"zeta_{suffix}"
    assert client.post("/register",json={"username":a,"password":pw}).status_code==201
    assert client.post("/register",json={"username":b,"password":pw}).status_code==201
    assert client.post("/register",json={"username":z,"password":pw}).status_code==201
    la=client.post("/login",json={"username":a,"password":pw}); lb=client.post("/login",json={"username":b,"password":pw})
    ha={"Authorization":f"Bearer {la.json()['access_token']}"}; hb={"Authorization":f"Bearer {lb.json()['access_token']}"}

    people=client.get("/users?page=1&limit=10&pin_creator=true",headers=ha); assert people.status_code==200
    assert people.json()["users"][0]["username"]=="Zytue67"
    assert sum(1 for x in people.json()["users"] if x["username"]=="Zytue67")==1
    jump=client.get("/users/letter/Z?limit=10&pin_creator=true",headers=ha).json(); assert jump["found"] is True
    # Z jump targets the first ordinary alphabetical Z account, never the pinned creator row.
    page=client.get(f"/users?page={jump['page']}&limit=10&pin_creator=true",headers=ha).json()["users"]
    assert page[jump["selected"]]["username"]==z

    assert client.post(f"/users/{b}/follow",headers=ha).status_code==200
    fol=client.get(f"/users/{b}/followers?page=1&limit=10",headers=ha).json()
    assert any(x["username"]==a for x in fol["users"])
    letter=client.get(f"/users/{b}/followers-letter/{a[0]}?limit=10",headers=ha).json(); assert letter["found"] is True

    for i in range(3):
        assert client.post(f"/messages/{b}",headers=ha,json={"body":f"hello {i}"}).status_code==201
    convs=client.get("/messages?page=1&limit=6",headers=hb).json()
    assert convs["total"]>=1 and convs["unread_total"]>=3
    win=client.get(f"/messages/{a}/window?offset=0&limit=2",headers=hb).json()
    assert win["total"]==3 and len(win["messages"])==2
    assert win["messages"][-1]["body"]=="hello 2"
    assert client.post(f"/messages/{a}/read",headers=hb).status_code==200
    assert client.get("/messages/unread-count",headers=hb).json()["unread"]==0
    assert client.delete(f"/messages/{a}",headers=hb).status_code==200
    assert all(x["username"]!=a for x in client.get("/messages?page=1&limit=6",headers=hb).json()["conversations"])
    # A new DM unhides a locally deleted conversation, as expected for real messaging.
    assert client.post(f"/messages/{b}",headers=ha,json={"body":"conversation returns"}).status_code==201
    assert any(x["username"]==a for x in client.get("/messages?page=1&limit=6",headers=hb).json()["conversations"])

    prefs=client.get("/notification-preferences",headers=ha).json(); assert prefs["master"] is True
    newprefs={"master":True,"messages":False,"replies":True,"forum_activity":False,"new_followers":True}
    assert client.put("/notification-preferences",headers=ha,json=newprefs).status_code==200
    assert client.get("/notification-preferences",headers=ha).json()==newprefs


def test_forum_delete_owner_only():
    suffix=uuid.uuid4().hex[:8]; pw="testpass123"
    owner=f"owner_{suffix}"; other=f"other_{suffix}"
    assert client.post("/register",json={"username":owner,"password":pw}).status_code==201
    assert client.post("/register",json={"username":other,"password":pw}).status_code==201
    to=client.post("/login",json={"username":owner,"password":pw}).json()["access_token"]
    tx=client.post("/login",json={"username":other,"password":pw}).json()["access_token"]
    ho={"Authorization":f"Bearer {to}"}; hx={"Authorization":f"Bearer {tx}"}
    f=client.post("/forums",headers=ho,json={"name":f"Owned_{suffix}"}); assert f.status_code==201
    fid=f.json()["id"]
    assert client.delete(f"/forums/{fid}",headers=hx).status_code==403
    assert client.delete(f"/forums/{fid}",headers=ho).status_code==200


def test_people_pagination_scales_and_letter_jump_ignores_pin():
    suffix=uuid.uuid4().hex[:5]; pw="testpass123"
    # Use a private temp DB and enough accounts to force several 10-user pages.
    names=[f"scale{i:02d}_{suffix}" for i in range(24)] + [f"zebra_{suffix}"]
    for name in names:
        assert client.post("/register",json={"username":name,"password":pw}).status_code==201
    login=client.post("/login",json={"username":names[0],"password":pw}); assert login.status_code==200
    h={"Authorization":f"Bearer {login.json()['access_token']}"}
    first=client.get("/users?page=1&limit=10&pin_creator=true",headers=h).json()
    assert first["users"][0]["username"]=="Zytue67"
    assert first["database_total"] >= 25
    last_page=(first["total"]+9)//10
    tail=client.get(f"/users?page={last_page}&limit=10&pin_creator=true",headers=h).json()
    assert tail["page"]==last_page and len(tail["users"])>=1
    jump=client.get("/users/letter/Z?limit=10&pin_creator=true",headers=h).json(); assert jump["found"] is True
    page=client.get(f"/users?page={jump['page']}&limit=10&pin_creator=true",headers=h).json()["users"]
    selected=page[jump["selected"]]["username"]
    assert selected.lower().startswith("z") and selected != "Zytue67"
