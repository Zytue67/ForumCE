from fastapi import Depends, FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from pydantic import BaseModel, Field
import sqlite3

from auth import create_access_token, decode_access_token, hash_password, verify_password
from database import create_tables, get_connection

app = FastAPI(title="ForumCE API", version="1.0")

# Local development website. Replace these with the production site origin when deployed.
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:5500",
        "http://localhost:5500",
        "https://forumce.com",
        "https://www.forumce.com",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)
security = HTTPBearer()
create_tables()

# ---------- request models ----------
class RegisterRequest(BaseModel):
    username: str = Field(min_length=3, max_length=20)
    password: str = Field(min_length=8, max_length=128)

class LoginRequest(BaseModel):
    username: str
    password: str

class BioUpdateRequest(BaseModel):
    bio: str = Field(max_length=160)

class ThreadCreateRequest(BaseModel):
    title: str = Field(min_length=1, max_length=40)
    body: str = Field(min_length=1, max_length=500)

class ForumCreateRequest(BaseModel):
    name: str = Field(min_length=1, max_length=32)

class BodyRequest(BaseModel):
    body: str = Field(min_length=1, max_length=500)

class MessageRequest(BaseModel):
    body: str = Field(min_length=1, max_length=500)

class NotificationPrefsRequest(BaseModel):
    master: bool
    messages: bool
    replies: bool
    forum_activity: bool
    new_followers: bool


def clean_nonempty(value: str, field: str, max_len: int):
    value = value.strip()
    if not value:
        raise HTTPException(400, f"{field} cannot be empty.")
    if len(value) > max_len:
        raise HTTPException(400, f"{field} is too long.")
    return value


def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    payload = decode_access_token(credentials.credentials)
    if payload is None:
        raise HTTPException(401, "Invalid or expired token.")
    try:
        user_id = int(payload["sub"])
    except (KeyError, ValueError, TypeError):
        raise HTTPException(401, "Invalid token.")
    c = get_connection()
    user = c.execute("SELECT id, username, bio, created_at FROM users WHERE id=?", (user_id,)).fetchone()
    c.close()
    if user is None:
        raise HTTPException(401, "User no longer exists.")
    return user


def user_by_username(c, username):
    return c.execute("SELECT id, username, bio, created_at FROM users WHERE username=?", (username.strip(),)).fetchone()


def notify(c, user_id, actor_id, kind, source_id, text):
    if user_id == actor_id:
        return
    prefs = c.execute("SELECT * FROM notification_preferences WHERE user_id=?", (user_id,)).fetchone()
    allowed = True
    if prefs:
        mapping = {"message": "messages", "reply": "replies", "forum": "forum_activity", "follow": "new_followers", "like": "forum_activity"}
        allowed = bool(prefs["master"]) and bool(prefs[mapping.get(kind, "forum_activity")])
    if allowed:
        c.execute("INSERT INTO notifications(user_id,actor_id,type,source_id,text) VALUES(?,?,?,?,?)",
                  (user_id, actor_id, kind, source_id, text))


def profile_dict(c, user, viewer_id=None):
    followers = c.execute("SELECT COUNT(*) n FROM follows WHERE following_id=?", (user["id"],)).fetchone()["n"]
    following = c.execute("SELECT COUNT(*) n FROM follows WHERE follower_id=?", (user["id"],)).fetchone()["n"]
    is_following = False
    if viewer_id and viewer_id != user["id"]:
        is_following = c.execute("SELECT 1 FROM follows WHERE follower_id=? AND following_id=?", (viewer_id, user["id"])).fetchone() is not None
    return {"id": user["id"], "username": user["username"], "bio": user["bio"], "created_at": user["created_at"],
            "followers": followers, "following": following, "is_following": is_following}


def pagination(page, limit):
    return (page - 1) * limit

# ---------- service ----------
@app.get("/health")
def health():
    return {"status": "online", "service": "ForumCE"}

@app.get("/")
def home():
    return {"name": "ForumCE", "version": "1.0", "status": "online"}

# ---------- accounts ----------
@app.post("/register", status_code=201)
def register(data: RegisterRequest):
    username = data.username.strip()
    if len(username) < 3 or not all(ch.isalnum() or ch == "_" for ch in username):
        raise HTTPException(400, "Username must be 3-20 characters using letters, numbers, or underscores.")
    c = get_connection()
    try:
        cur = c.execute("INSERT INTO users(username,password_hash) VALUES(?,?)", (username, hash_password(data.password)))
        c.execute("INSERT INTO notification_preferences(user_id) VALUES(?)", (cur.lastrowid,))
        c.commit()
    except sqlite3.IntegrityError:
        c.close(); raise HTTPException(409, "Username already exists.")
    user_id = cur.lastrowid
    c.close()
    return {"id": user_id, "username": username, "bio": ""}

@app.post("/login")
def login(data: LoginRequest):
    c = get_connection()
    user = c.execute("SELECT * FROM users WHERE username=?", (data.username.strip(),)).fetchone()
    c.close()
    if not user or not verify_password(data.password, user["password_hash"]):
        raise HTTPException(401, "Invalid username or password.")
    return {"access_token": create_access_token(user["id"], user["username"]), "token_type": "bearer",
            "user": {"id": user["id"], "username": user["username"], "bio": user["bio"], "created_at": user["created_at"]}}

@app.get("/me")
def me(user=Depends(get_current_user)):
    c = get_connection(); result = profile_dict(c, user, user["id"]); c.close(); return result

@app.put("/me/bio")
def update_bio(data: BioUpdateRequest, user=Depends(get_current_user)):
    bio = data.bio.strip()
    c = get_connection(); c.execute("UPDATE users SET bio=? WHERE id=?", (bio, user["id"])); c.commit(); c.close()
    return {"id": user["id"], "username": user["username"], "bio": bio}

# ---------- users / follows ----------

def _people_rows(c):
    return c.execute("SELECT id,username,bio,created_at FROM users ORDER BY username COLLATE NOCASE,id").fetchall()

@app.get("/users")
def list_users(page: int=Query(1, ge=1), limit: int=Query(10, ge=1, le=10), pin_creator: bool=True, user=Depends(get_current_user)):
    c=get_connection(); alpha=list(_people_rows(c)); creator=next((r for r in alpha if r["username"].lower()=="zytue67"),None)
    # Pin the creator once at the front, but do not duplicate that account in
    # the normal alphabetical sequence. This keeps the visible list equal to
    # the real database population while reserving position 0 for Zytue67.
    alphabetical=[r for r in alpha if not (pin_creator and creator is not None and r["id"]==creator["id"])]
    virtual=([creator] if pin_creator and creator is not None else []) + alphabetical
    total=len(virtual); off=pagination(page,limit); rows=virtual[off:off+limit]
    out=[profile_dict(c,r,user["id"]) for r in rows]; c.close()
    return {"users":out,"page":page,"limit":limit,"total":total,"database_total":len(alpha),"has_more":page*limit<total}

@app.get("/users/letter/{letter}")
def users_letter(letter: str, limit: int=Query(10, ge=1, le=10), pin_creator: bool=True, user=Depends(get_current_user)):
    ch=(letter or "")[:1].lower(); c=get_connection(); alpha=list(_people_rows(c)); creator=next((r for r in alpha if r["username"].lower()=="zytue67"),None)
    alphabetical=[r for r in alpha if not (pin_creator and creator is not None and r["id"]==creator["id"])]
    base=1 if pin_creator and creator is not None else 0
    # Letter jumps intentionally ignore the pinned creator copy. Therefore Z
    # lands on the first ordinary alphabetical Z account, not the pinned row.
    index=next((i for i,r in enumerate(alphabetical) if r["username"].lower().startswith(ch)),None) if ch else None
    c.close()
    if index is None: return {"found":False}
    virtual_index=base+index
    return {"found":True,"page":virtual_index//limit+1,"selected":virtual_index%limit}

@app.get("/users/search")
def search_users(q: str, user=Depends(get_current_user)):
    q=q.strip()
    if not q: return {"users": []}
    c = get_connection()
    rows = c.execute("""SELECT id,username,bio,created_at FROM users WHERE id!=? AND username LIKE ?
        ORDER BY CASE WHEN username=? COLLATE NOCASE THEN 0 ELSE 1 END, username COLLATE NOCASE LIMIT 8""", (user["id"],f"%{q}%",q)).fetchall()
    out = [profile_dict(c, r, user["id"]) for r in rows]; c.close(); return {"users": out}

@app.get("/users/{username}")
def get_profile(username: str, user=Depends(get_current_user)):
    c = get_connection(); target = user_by_username(c, username)
    if not target: c.close(); raise HTTPException(404, "User not found.")
    out = profile_dict(c, target, user["id"]); c.close(); return out

@app.post("/users/{username}/follow")
def follow(username: str, user=Depends(get_current_user)):
    c = get_connection(); target = user_by_username(c, username)
    if not target: c.close(); raise HTTPException(404, "User not found.")
    if target["id"] == user["id"]: c.close(); raise HTTPException(400, "You cannot follow yourself.")
    try:
        c.execute("INSERT INTO follows(follower_id,following_id) VALUES(?,?)", (user["id"], target["id"]))
        notify(c, target["id"], user["id"], "follow", user["id"], f'{user["username"]} followed you')
        c.commit()
    except sqlite3.IntegrityError:
        c.close(); raise HTTPException(409, "Already following this user.")
    c.close(); return {"following": True}

@app.delete("/users/{username}/follow")
def unfollow(username: str, user=Depends(get_current_user)):
    c = get_connection(); target = user_by_username(c, username)
    if not target: c.close(); raise HTTPException(404, "User not found.")
    cur = c.execute("DELETE FROM follows WHERE follower_id=? AND following_id=?", (user["id"], target["id"])); c.commit(); c.close()
    return {"following": False, "changed": cur.rowcount > 0}

def _relation_rows(c,target_id,following_tab):
    if following_tab:
        return c.execute("""SELECT u.id,u.username,u.bio,u.created_at FROM follows f JOIN users u ON u.id=f.following_id
            WHERE f.follower_id=? ORDER BY u.username COLLATE NOCASE,u.id""",(target_id,)).fetchall()
    return c.execute("""SELECT u.id,u.username,u.bio,u.created_at FROM follows f JOIN users u ON u.id=f.follower_id
            WHERE f.following_id=? ORDER BY u.username COLLATE NOCASE,u.id""",(target_id,)).fetchall()

def _relation_payload(username,page,limit,following_tab,viewer):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    all_rows=list(_relation_rows(c,target["id"],following_tab)); total=len(all_rows); rows=all_rows[pagination(page,limit):pagination(page,limit)+limit]
    out=[profile_dict(c,r,viewer["id"]) for r in rows]; c.close(); return {"users":out,"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.get("/users/{username}/followers")
def followers(username: str, page: int=Query(1, ge=1), limit: int=Query(10, ge=1, le=10), user=Depends(get_current_user)):
    return _relation_payload(username,page,limit,False,user)

@app.get("/users/{username}/following")
def following(username: str, page: int=Query(1, ge=1), limit: int=Query(10, ge=1, le=10), user=Depends(get_current_user)):
    return _relation_payload(username,page,limit,True,user)

@app.get("/users/{username}/followers-letter/{letter}")
def followers_letter(username:str,letter:str,limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    rows=list(_relation_rows(c,target["id"],False)); ch=(letter or "")[:1].lower(); idx=next((i for i,r in enumerate(rows) if r["username"].lower().startswith(ch)),None); c.close()
    return {"found":idx is not None, **({"page":idx//limit+1,"selected":idx%limit} if idx is not None else {})}

@app.get("/users/{username}/following-letter/{letter}")
def following_letter(username:str,letter:str,limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    rows=list(_relation_rows(c,target["id"],True)); ch=(letter or "")[:1].lower(); idx=next((i for i,r in enumerate(rows) if r["username"].lower().startswith(ch)),None); c.close()
    return {"found":idx is not None, **({"page":idx//limit+1,"selected":idx%limit} if idx is not None else {})}

# ---------- forums ----------
@app.get("/forums")
def forums():
    c=get_connection()
    rows=c.execute("""
      SELECT f.id, f.name,
        (SELECT t.id FROM threads t WHERE t.forum_id=f.id ORDER BY t.id ASC LIMIT 1) thread_id,
        COALESCE((SELECT u.username FROM threads t JOIN users u ON u.id=t.author_id WHERE t.forum_id=f.id ORDER BY t.id ASC LIMIT 1),'ForumCE') author,
        COALESCE(f.updated_at,'') last_activity
      FROM forums f
      ORDER BY CASE WHEN f.id=1 THEN 0 ELSE 1 END,
               f.updated_at DESC,
               f.id DESC
    """).fetchall()
    c.close()
    return {"forums":[dict(r) for r in rows]}

@app.post("/forums", status_code=201)
def create_forum(data: ForumCreateRequest, user=Depends(get_current_user)):
    name=clean_nonempty(data.name,"Forum name",32)
    c=get_connection()
    try:
        cur=c.execute("INSERT INTO forums(name,created_by) VALUES(?,?)",(name,user["id"]))
        forum_id=cur.lastrowid
        tcur=c.execute("INSERT INTO threads(forum_id,author_id,title,body) VALUES(?,?,?,?)",
                       (forum_id,user["id"],name,""))
        thread_id=tcur.lastrowid
        c.commit()
    except sqlite3.IntegrityError:
        c.rollback(); c.close(); raise HTTPException(409,"Forum name already exists.")
    except Exception:
        c.rollback(); c.close(); raise
    c.close()
    return {"id":forum_id,"thread_id":thread_id,"name":name}

@app.delete("/forums/{forum_id}")
def delete_forum(forum_id:int, user=Depends(get_current_user)):
    if forum_id == 1:
        raise HTTPException(403, "The Welcome forum is pinned and cannot be deleted.")
    c=get_connection()
    forum=c.execute("SELECT id,name,created_by FROM forums WHERE id=?",(forum_id,)).fetchone()
    if not forum:
        c.close(); raise HTTPException(404,"Forum not found.")
    # Forum owners may delete their own forum. The creator account can clean up
    # legacy forums that predate created_by.
    is_creator = str(user["username"]).lower() == "zytue67"
    if forum["created_by"] not in (None, user["id"]) and not is_creator:
        c.close(); raise HTTPException(403,"You cannot delete this forum.")
    if forum["created_by"] is None and not is_creator:
        c.close(); raise HTTPException(403,"Only the ForumCE creator can delete this legacy forum.")
    c.execute("DELETE FROM forums WHERE id=?",(forum_id,))
    c.commit(); c.close()
    return {"deleted":True,"id":forum_id}

@app.get("/forums/{forum_id}/threads")
def list_threads(forum_id:int,page:int=Query(1,ge=1),limit:int=Query(10,ge=1,le=10)):
    c=get_connection()
    if not c.execute("SELECT 1 FROM forums WHERE id=?",(forum_id,)).fetchone(): c.close(); raise HTTPException(404,"Forum not found.")
    total=c.execute("SELECT COUNT(*) n FROM threads WHERE forum_id=?",(forum_id,)).fetchone()["n"]
    rows=c.execute("""SELECT t.*,u.username,(SELECT COUNT(*) FROM posts p WHERE p.thread_id=t.id) post_count
        FROM threads t JOIN users u ON u.id=t.author_id WHERE t.forum_id=? ORDER BY t.id DESC LIMIT ? OFFSET ?""",(forum_id,limit,pagination(page,limit))).fetchall()
    c.close(); return {"threads":[dict(r) for r in rows],"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.post("/forums/{forum_id}/threads",status_code=201)
def create_thread(forum_id:int,data:ThreadCreateRequest,user=Depends(get_current_user)):
    title=clean_nonempty(data.title,"Title",40); body=clean_nonempty(data.body,"Body",500); c=get_connection()
    if not c.execute("SELECT 1 FROM forums WHERE id=?",(forum_id,)).fetchone(): c.close(); raise HTTPException(404,"Forum not found.")
    try:
        cur=c.execute("INSERT INTO threads(forum_id,author_id,title,body) VALUES(?,?,?,?)",(forum_id,user["id"],title,body))
        tid=cur.lastrowid
        post_cur=c.execute("INSERT INTO posts(thread_id,author_id,body) VALUES(?,?,?)",(tid,user["id"],body))
        post_id=post_cur.lastrowid
        c.execute("UPDATE forums SET updated_at=STRFTIME('%Y-%m-%d %H:%M:%f','now') WHERE id=?",(forum_id,))
        c.commit()
    except Exception:
        c.rollback()
        c.close()
        raise
    c.close()
    return {"id":tid,"forum_id":forum_id,"title":title,"body":body,"root_post_id":post_id}

@app.get("/threads/{thread_id}")
def thread(thread_id:int):
    c=get_connection(); r=c.execute("""SELECT t.*,u.username,
      (SELECT p.id FROM posts p WHERE p.thread_id=t.id ORDER BY p.id ASC LIMIT 1) root_post_id
      FROM threads t JOIN users u ON u.id=t.author_id WHERE t.id=?""",(thread_id,)).fetchone(); c.close()
    if not r: raise HTTPException(404,"Thread not found.")
    return dict(r)

@app.get("/threads/{thread_id}/posts")
def list_posts(thread_id:int,page:int=Query(1,ge=1),limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection()
    if not c.execute("SELECT 1 FROM threads WHERE id=?",(thread_id,)).fetchone(): c.close(); raise HTTPException(404,"Thread not found.")
    total=c.execute("SELECT COUNT(*) n FROM posts WHERE thread_id=?",(thread_id,)).fetchone()["n"]
    rows=c.execute("""SELECT p.id,p.thread_id,p.author_id,u.username,p.body,p.created_at,
      (SELECT COUNT(*) FROM post_likes l WHERE l.post_id=p.id) like_count,
      EXISTS(SELECT 1 FROM post_likes l WHERE l.post_id=p.id AND l.user_id=?) liked,
      (SELECT COUNT(*) FROM replies r WHERE r.post_id=p.id) reply_count
      FROM posts p JOIN users u ON u.id=p.author_id WHERE p.thread_id=? ORDER BY p.id ASC LIMIT ? OFFSET ?""",
      (user["id"],thread_id,limit,pagination(page,limit))).fetchall(); c.close()
    return {"posts":[dict(r) for r in rows],"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.post("/threads/{thread_id}/posts",status_code=201)
def create_post(thread_id:int,data:BodyRequest,user=Depends(get_current_user)):
    body=clean_nonempty(data.body,"Post",500); c=get_connection(); th=c.execute("SELECT * FROM threads WHERE id=?",(thread_id,)).fetchone()
    if not th: c.close(); raise HTTPException(404,"Thread not found.")
    cur=c.execute("INSERT INTO posts(thread_id,author_id,body) VALUES(?,?,?)",(thread_id,user["id"],body)); notify(c,th["author_id"],user["id"],"forum",cur.lastrowid,f'{user["username"]} posted in your thread'); c.execute("UPDATE forums SET updated_at=STRFTIME('%Y-%m-%d %H:%M:%f','now') WHERE id=?",(th["forum_id"],)); c.commit(); pid=cur.lastrowid; c.close(); return {"id":pid,"thread_id":thread_id,"body":body}

@app.get("/posts/{post_id}/replies")
def list_replies(post_id:int,page:int=Query(1,ge=1),limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection()
    if not c.execute("SELECT 1 FROM posts WHERE id=?",(post_id,)).fetchone(): c.close(); raise HTTPException(404,"Post not found.")
    total=c.execute("SELECT COUNT(*) n FROM replies WHERE post_id=?",(post_id,)).fetchone()["n"]
    rows=c.execute("""SELECT r.id,r.post_id,r.author_id,u.username,r.body,r.created_at,
      (SELECT COUNT(*) FROM reply_likes l WHERE l.reply_id=r.id) like_count,
      EXISTS(SELECT 1 FROM reply_likes l WHERE l.reply_id=r.id AND l.user_id=?) liked
      FROM replies r JOIN users u ON u.id=r.author_id WHERE r.post_id=? ORDER BY r.id ASC LIMIT ? OFFSET ?""",
      (user["id"],post_id,limit,pagination(page,limit))).fetchall(); c.close()
    return {"replies":[dict(r) for r in rows],"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.post("/posts/{post_id}/replies",status_code=201)
def create_reply(post_id:int,data:BodyRequest,user=Depends(get_current_user)):
    body=clean_nonempty(data.body,"Reply",500); c=get_connection(); post=c.execute("SELECT p.*,t.forum_id FROM posts p JOIN threads t ON t.id=p.thread_id WHERE p.id=?",(post_id,)).fetchone()
    if not post: c.close(); raise HTTPException(404,"Post not found.")
    cur=c.execute("INSERT INTO replies(post_id,author_id,body) VALUES(?,?,?)",(post_id,user["id"],body)); notify(c,post["author_id"],user["id"],"reply",cur.lastrowid,f'{user["username"]} replied to your post'); c.execute("UPDATE forums SET updated_at=STRFTIME('%Y-%m-%d %H:%M:%f','now') WHERE id=?",(post["forum_id"],)); c.commit(); rid=cur.lastrowid; c.close(); return {"id":rid,"post_id":post_id,"body":body}

@app.post("/posts/{post_id}/like")
def like_post(post_id:int,user=Depends(get_current_user)):
    c=get_connection(); post=c.execute("SELECT * FROM posts WHERE id=?",(post_id,)).fetchone()
    if not post: c.close(); raise HTTPException(404,"Post not found.")
    existing=c.execute("SELECT 1 FROM post_likes WHERE post_id=? AND user_id=?",(post_id,user["id"])).fetchone()
    if existing: c.execute("DELETE FROM post_likes WHERE post_id=? AND user_id=?",(post_id,user["id"])); liked=False
    else: c.execute("INSERT INTO post_likes(post_id,user_id) VALUES(?,?)",(post_id,user["id"])); notify(c,post["author_id"],user["id"],"like",post_id,f'{user["username"]} liked your post'); liked=True
    c.commit(); count=c.execute("SELECT COUNT(*) n FROM post_likes WHERE post_id=?",(post_id,)).fetchone()["n"]; c.close(); return {"liked":liked,"like_count":count}

@app.post("/replies/{reply_id}/like")
def like_reply(reply_id:int,user=Depends(get_current_user)):
    c=get_connection(); reply=c.execute("SELECT * FROM replies WHERE id=?",(reply_id,)).fetchone()
    if not reply: c.close(); raise HTTPException(404,"Reply not found.")
    existing=c.execute("SELECT 1 FROM reply_likes WHERE reply_id=? AND user_id=?",(reply_id,user["id"])).fetchone()
    if existing: c.execute("DELETE FROM reply_likes WHERE reply_id=? AND user_id=?",(reply_id,user["id"])); liked=False
    else: c.execute("INSERT INTO reply_likes(reply_id,user_id) VALUES(?,?)",(reply_id,user["id"])); notify(c,reply["author_id"],user["id"],"like",reply_id,f'{user["username"]} liked your reply'); liked=True
    c.commit(); count=c.execute("SELECT COUNT(*) n FROM reply_likes WHERE reply_id=?",(reply_id,)).fetchone()["n"]; c.close(); return {"liked":liked,"like_count":count}

# ---------- direct messages ----------
def conversation_for(c,a,b,create=False):
    lo,hi=sorted((a,b)); row=c.execute("SELECT * FROM conversations WHERE user1_id=? AND user2_id=?",(lo,hi)).fetchone()
    if not row and create:
        cur=c.execute("INSERT INTO conversations(user1_id,user2_id) VALUES(?,?)",(lo,hi)); cid=cur.lastrowid
        c.execute("INSERT INTO conversation_state(conversation_id,user_id) VALUES(?,?),(?,?)",(cid,lo,cid,hi)); row=c.execute("SELECT * FROM conversations WHERE id=?",(cid,)).fetchone()
    return row

@app.get("/messages")
def conversations(page:int=Query(1,ge=1),limit:int=Query(6,ge=1,le=6),user=Depends(get_current_user)):
    c=get_connection(); base="""SELECT cv.id,cv.created_at,
      CASE WHEN cv.user1_id=? THEN u2.id ELSE u1.id END other_id,
      CASE WHEN cv.user1_id=? THEN u2.username ELSE u1.username END username,
      (SELECT body FROM messages m WHERE m.conversation_id=cv.id ORDER BY m.id DESC LIMIT 1) last_message,
      (SELECT sender_id FROM messages m WHERE m.conversation_id=cv.id ORDER BY m.id DESC LIMIT 1) last_sender_id,
      (SELECT created_at FROM messages m WHERE m.conversation_id=cv.id ORDER BY m.id DESC LIMIT 1) last_message_at,
      (SELECT COUNT(*) FROM messages m WHERE m.conversation_id=cv.id AND m.sender_id!=? AND m.id>COALESCE(cs.last_read_message_id,0)) unread_count
      FROM conversations cv JOIN users u1 ON u1.id=cv.user1_id JOIN users u2 ON u2.id=cv.user2_id
      JOIN conversation_state cs ON cs.conversation_id=cv.id AND cs.user_id=?
      WHERE (cv.user1_id=? OR cv.user2_id=?) AND cs.hidden=0
      AND EXISTS(SELECT 1 FROM messages m WHERE m.conversation_id=cv.id)"""
    params=(user["id"],user["id"],user["id"],user["id"],user["id"],user["id"])
    total=c.execute("SELECT COUNT(*) n FROM ("+base+")",params).fetchone()["n"]
    rows=c.execute(base+" ORDER BY COALESCE(last_message_at,cv.created_at) DESC,cv.id DESC LIMIT ? OFFSET ?",params+(limit,pagination(page,limit))).fetchall()
    unread=c.execute("""SELECT COUNT(*) n FROM messages m JOIN conversations cv ON cv.id=m.conversation_id
      JOIN conversation_state cs ON cs.conversation_id=cv.id AND cs.user_id=?
      WHERE (cv.user1_id=? OR cv.user2_id=?) AND cs.hidden=0 AND m.sender_id!=? AND m.id>COALESCE(cs.last_read_message_id,0)""",(user["id"],user["id"],user["id"],user["id"])).fetchone()["n"]
    out=[]
    for r in rows:
        item=dict(r); item["last_mine"] = item.get("last_sender_id") == user["id"]; out.append(item)
    c.close(); return {"conversations":out,"page":page,"limit":limit,"total":total,"unread_total":unread,"has_more":page*limit<total}

@app.get("/messages/unread-count")
def unread_messages(user=Depends(get_current_user)):
    c=get_connection(); n=c.execute("""SELECT COUNT(*) n FROM messages m JOIN conversations cv ON cv.id=m.conversation_id
      JOIN conversation_state cs ON cs.conversation_id=cv.id AND cs.user_id=?
      WHERE (cv.user1_id=? OR cv.user2_id=?) AND cs.hidden=0 AND m.sender_id!=? AND m.id>COALESCE(cs.last_read_message_id,0)""",(user["id"],user["id"],user["id"],user["id"])).fetchone()["n"]; c.close(); return {"unread":n}

@app.get("/messages/{username}/window")
def message_window(username:str,offset:int=Query(0,ge=0),limit:int=Query(3,ge=1,le=3),user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    if target["id"]==user["id"]: c.close(); raise HTTPException(400,"You cannot message yourself.")
    cv=conversation_for(c,user["id"],target["id"],False)
    if not cv: c.close(); return {"conversation_id":None,"created_at":None,"messages":[],"offset":offset,"total":0}
    total=c.execute("SELECT COUNT(*) n FROM messages WHERE conversation_id=?",(cv["id"],)).fetchone()["n"]
    rows=c.execute("""SELECT * FROM (SELECT m.id,m.sender_id,u.username,m.body,m.created_at FROM messages m JOIN users u ON u.id=m.sender_id
      WHERE m.conversation_id=? ORDER BY m.id DESC LIMIT ? OFFSET ?) x ORDER BY id ASC""",(cv["id"],limit,offset)).fetchall(); c.close()
    out=[]
    for r in rows:
        item=dict(r); item["mine"] = item.get("sender_id") == user["id"]; out.append(item)
    return {"conversation_id":cv["id"],"created_at":cv["created_at"],"messages":out,"offset":offset,"total":total}

@app.get("/messages/{username}")
def message_history(username:str,page:int=Query(1,ge=1),limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    if target["id"]==user["id"]: c.close(); raise HTTPException(400,"You cannot message yourself.")
    cv=conversation_for(c,user["id"],target["id"],False)
    if not cv: c.close(); return {"conversation_id":None,"created_at":None,"messages":[],"page":page,"limit":limit,"total":0,"has_more":False}
    total=c.execute("SELECT COUNT(*) n FROM messages WHERE conversation_id=?",(cv["id"],)).fetchone()["n"]
    rows=c.execute("""SELECT * FROM (SELECT m.id,m.sender_id,u.username,m.body,m.created_at FROM messages m JOIN users u ON u.id=m.sender_id
      WHERE m.conversation_id=? ORDER BY m.id DESC LIMIT ? OFFSET ?) x ORDER BY id ASC""",(cv["id"],limit,pagination(page,limit))).fetchall(); c.close()
    return {"conversation_id":cv["id"],"created_at":cv["created_at"],"messages":[dict(r) for r in rows],"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.post("/messages/{username}",status_code=201)
def send_message(username:str,data:MessageRequest,user=Depends(get_current_user)):
    body=clean_nonempty(data.body,"Message",500); c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    if target["id"]==user["id"]: c.close(); raise HTTPException(400,"You cannot message yourself.")
    cv=conversation_for(c,user["id"],target["id"],True); cur=c.execute("INSERT INTO messages(conversation_id,sender_id,body) VALUES(?,?,?)",(cv["id"],user["id"],body))
    c.execute("UPDATE conversation_state SET hidden=0 WHERE conversation_id=? AND user_id IN (?,?)",(cv["id"],user["id"],target["id"]))
    c.execute("UPDATE conversation_state SET last_read_message_id=? WHERE conversation_id=? AND user_id=?",(cur.lastrowid,cv["id"],user["id"]))
    notify(c,target["id"],user["id"],"message",cur.lastrowid,f'New message from {user["username"]}'); c.commit(); mid=cur.lastrowid; c.close(); return {"id":mid,"conversation_id":cv["id"],"body":body}

@app.post("/messages/{username}/read")
def mark_messages_read(username:str,user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    cv=conversation_for(c,user["id"],target["id"],False)
    if not cv: c.close(); return {"read":True}
    last=c.execute("SELECT MAX(id) id FROM messages WHERE conversation_id=?",(cv["id"],)).fetchone()["id"]
    c.execute("UPDATE conversation_state SET last_read_message_id=? WHERE conversation_id=? AND user_id=?",(last,cv["id"],user["id"])); c.commit(); c.close(); return {"read":True}

@app.delete("/messages/{username}")
def hide_conversation(username:str,user=Depends(get_current_user)):
    c=get_connection(); target=user_by_username(c,username)
    if not target: c.close(); raise HTTPException(404,"User not found.")
    cv=conversation_for(c,user["id"],target["id"],False)
    if cv: c.execute("UPDATE conversation_state SET hidden=1 WHERE conversation_id=? AND user_id=?",(cv["id"],user["id"])); c.commit()
    c.close(); return {"deleted":True}

# ---------- notifications ----------
@app.get("/notifications")
def list_notifications(page:int=Query(1,ge=1),limit:int=Query(10,ge=1,le=10),user=Depends(get_current_user)):
    c=get_connection(); total=c.execute("SELECT COUNT(*) n FROM notifications WHERE user_id=?",(user["id"],)).fetchone()["n"]
    rows=c.execute("""SELECT n.*,u.username actor_username FROM notifications n LEFT JOIN users u ON u.id=n.actor_id
      WHERE n.user_id=? ORDER BY n.id DESC LIMIT ? OFFSET ?""",(user["id"],limit,pagination(page,limit))).fetchall(); c.close()
    return {"notifications":[dict(r) for r in rows],"page":page,"limit":limit,"total":total,"has_more":page*limit<total}

@app.get("/notifications/unread-count")
def unread_notifications(user=Depends(get_current_user)):
    c=get_connection(); n=c.execute("SELECT COUNT(*) n FROM notifications WHERE user_id=? AND read=0",(user["id"],)).fetchone()["n"]; c.close(); return {"unread":n}

@app.post("/notifications/read-all")
def read_all_notifications(user=Depends(get_current_user)):
    c=get_connection(); c.execute("UPDATE notifications SET read=1 WHERE user_id=?",(user["id"],)); c.commit(); c.close(); return {"read":True}

@app.get("/notification-preferences")
def get_notification_preferences(user=Depends(get_current_user)):
    c=get_connection(); p=c.execute("SELECT master,messages,replies,forum_activity,new_followers FROM notification_preferences WHERE user_id=?",(user["id"],)).fetchone(); c.close(); return {k:bool(p[k]) for k in p.keys()}

@app.put("/notification-preferences")
def set_notification_preferences(data:NotificationPrefsRequest,user=Depends(get_current_user)):
    c=get_connection(); c.execute("""INSERT INTO notification_preferences(user_id,master,messages,replies,forum_activity,new_followers) VALUES(?,?,?,?,?,?)
      ON CONFLICT(user_id) DO UPDATE SET master=excluded.master,messages=excluded.messages,replies=excluded.replies,forum_activity=excluded.forum_activity,new_followers=excluded.new_followers""",
      (user["id"],int(data.master),int(data.messages),int(data.replies),int(data.forum_activity),int(data.new_followers))); c.commit(); c.close(); return data.model_dump()
