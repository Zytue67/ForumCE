import glob
import getpass
import json
import os
import time
from datetime import datetime
from urllib.parse import quote
import urllib.error
import urllib.request
from datetime import datetime, timezone
import serial

BAUD_RATE = 9600
SERIAL_TIMEOUT = 0.20
API_URL = "https://api.forumce.com"
TOKEN_FILE = os.path.join(os.path.dirname(__file__), ".forumce_token")


def find_calculator():
    devices = sorted(glob.glob("/dev/cu.usbmodem*"))
    return devices[0] if devices else None


def send_line(calc, text):
    payload = (text + "\n").encode("utf-8")
    sent = 0
    while sent < len(payload):
        written = calc.write(payload[sent:])
        if not written:
            raise serial.SerialTimeoutException("ForumCE serial write timed out")
        sent += written
    calc.flush()
    # Avoid echoing private DM/post bodies in the terminal.
    head = text.split("|", 1)[0]
    if head in {"POST", "REPLY", "DM", "CONV", "PERSON", "PROFILEDATA"}:
        print(f"Bridge -> {head}|...")
    else:
        print(f"Bridge -> {text}")


def clean_field(value, max_len=None):
    text = str(value or "").replace("|", "/").replace("\n", " ").replace("\r", " ")
    return text[:max_len] if max_len else text


def save_token(token):
    with open(TOKEN_FILE, "w", encoding="utf-8") as f:
        f.write(token)


def load_token():
    try:
        with open(TOKEN_FILE, "r", encoding="utf-8") as f:
            return f.read().strip()
    except FileNotFoundError:
        return None


def delete_token():
    try:
        os.remove(TOKEN_FILE)
    except FileNotFoundError:
        pass


def api_request(method, path, token=None, body=None):
    headers = {"Accept": "application/json"}
    data = None
    if token:
        headers["Authorization"] = f"Bearer {token}"
    if body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(body).encode("utf-8")
    headers["User-Agent"] = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36"
    request = urllib.request.Request(API_URL + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=4) as response:
            raw = response.read()
            return response.status, json.loads(raw.decode("utf-8")) if raw else {}
    except urllib.error.HTTPError as error:
        try:
            payload = json.loads(error.read().decode("utf-8"))
        except Exception:
            payload = {}
        return error.code, payload
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
        return 0, {}


def server_online():
    status, data = api_request("GET", "/health")
    return status == 200 and data.get("status") == "online"

def local_timestamp(value):
    raw = str(value or "").strip()

    if not raw:
        return ""

    try:
        # SQLite CURRENT_TIMESTAMP is UTC but has no timezone suffix.
        dt = datetime.fromisoformat(raw.replace("Z", "+00:00"))

        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)

        # Convert automatically using the Mac's current timezone,
        # including daylight-saving time.
        return dt.astimezone().strftime("%Y-%m-%d %H:%M")

    except Exception:
        return raw[:16]

def login_interactive():
    print("\nForumCE Bridge Login\n--------------------")
    username = input("Username: ").strip()
    password = getpass.getpass("Password: ")
    status, data = api_request("POST", "/login", body={"username": username, "password": password})
    if status != 200:
        print("Login failed:", data.get("detail", "Unknown error"))
        return None
    token = data.get("access_token")
    if not token:
        return None
    save_token(token)
    print(f"Logged in as {data['user']['username']}")
    return token


def authenticated_user(token):
    status, data = api_request("GET", "/me", token=token)
    return data if status == 200 else None


def user_role(user):
    return "CREATOR" if user.get("username", "").lower() == "zytue67" else "USER"


def emit_person(calc, person):
    send_line(calc, "PERSON|{}|{}|{}|{}|{}|{}".format(
        int(person.get("id", 0)),
        clean_field(person.get("username"), 20),
        int(person.get("followers", 0)),
        int(person.get("following", 0)),
        1 if person.get("is_following") else 0,
        clean_field(person.get("bio"), 160),
    ))


def emit_profile(calc, person):
    send_line(calc, "PROFILEDATA|{}|{}|{}|{}|{}|{}".format(
        int(person.get("id", 0)),
        clean_field(person.get("username"), 20),
        int(person.get("followers", 0)),
        int(person.get("following", 0)),
        1 if person.get("is_following") else 0,
        clean_field(person.get("bio"), 160),
    ))


def action_result(calc, status, ok_status=(200, 201)):
    send_line(calc, "ACTION_OK" if status in ok_status else f"ACTION_ERR|{status}")


def handle_command(calc, command, token):
    head = command.split("|", 1)[0]
    if head in {"NEWPOST", "NEWREPLY", "SENDDM", "BIO"}:
        print(f"TI-84 -> {head}|...")
    else:
        print(f"TI-84 -> {command}")

    if command == "PING":
        send_line(calc, "PONG")
    elif command == "STATUS":
        send_line(calc, "ONLINE" if server_online() else "OFFLINE")
    elif command == "ME":
        if not token:
            send_line(calc, "AUTH_REQUIRED")
            return None
        user = authenticated_user(token)
        if user is None:
            delete_token()
            send_line(calc, "AUTH_REQUIRED")
            return None
        send_line(calc, f"USER|{user['id']}|{user['username']}|{user_role(user)}")
    elif command == "TIME":
        now = datetime.now()
        send_line(calc, f"CLOCK|{now.strftime('%m/%d')}|{now.strftime('%H:%M')}")
    elif command == "UNREAD":
        status, data = api_request("GET", "/messages/unread-count", token=token)
        send_line(calc, f"UNREAD|{int(data.get('unread', 0))}" if status == 200 else "ERR|UNREAD")

    # ----- forums / threads -----
    elif command.startswith("NEWPOST|"):
        try:
            _, thread_text, body = command.split("|", 2)
            status, _ = api_request("POST", f"/threads/{int(thread_text)}/posts", token=token, body={"body": body})
            action_result(calc, status, (201,))
        except Exception:
            send_line(calc, "ACTION_ERR|NEWPOST")
    elif command.startswith("NEWREPLY|"):
        try:
            _, post_text, body = command.split("|", 2)
            status, _ = api_request("POST", f"/posts/{int(post_text)}/replies", token=token, body={"body": body})
            action_result(calc, status, (201,))
        except Exception:
            send_line(calc, "ACTION_ERR|NEWREPLY")
    elif command.startswith("LIKEPOST|"):
        try:
            post_id = int(command.split("|", 1)[1])
            status, _ = api_request("POST", f"/posts/{post_id}/like", token=token)
            action_result(calc, status, (200,))
        except Exception:
            send_line(calc, "ACTION_ERR|LIKEPOST")
    elif command.startswith("LIKEREPLY|"):
        try:
            reply_id = int(command.split("|", 1)[1])
            status, _ = api_request("POST", f"/replies/{reply_id}/like", token=token)
            action_result(calc, status, (200,))
        except Exception:
            send_line(calc, "ACTION_ERR|LIKEREPLY")
    elif command.startswith("NEWFORUM|"):
        try:
            _, name = command.split("|", 1)
            status, _ = api_request("POST", "/forums", token=token, body={"name": name})
            action_result(calc, status, (201,))
        except Exception:
            send_line(calc, "ACTION_ERR|NEWFORUM")
    elif command.startswith("DELETEFORUM|"):
        try:
            forum_id = int(command.split("|", 1)[1])
            status, _ = api_request("DELETE", f"/forums/{forum_id}", token=token)
            action_result(calc, status, (200,))
        except Exception:
            send_line(calc, "ACTION_ERR|DELETEFORUM")
    elif command.startswith("REPLIES|"):
        try:
            _, post_text, page_text = command.split("|", 2)
            post_id = int(post_text); page = max(1, int(page_text))
        except (ValueError, TypeError):
            send_line(calc, "ERR|REPLIES"); return token
        status, data = api_request("GET", f"/posts/{post_id}/replies?page={page}&limit=10", token=token)
        if status != 200:
            send_line(calc, "ERR|REPLIES")
        else:
            replies = data.get("replies", [])
            send_line(calc, f"REPLY_COUNT|{data.get('total', len(replies))}|{page}")
            for reply in replies:
                send_line(calc, f"REPLY|{reply['id']}|{post_id}|{clean_field(reply.get('username') or '?',20)}|{clean_field(local_timestamp(reply.get('created_at')))}|{int(reply.get('like_count',0))}|{1 if reply.get('liked') else 0}|{clean_field(reply.get('body'),319)}")
            send_line(calc, "REPLIES_END")
    elif command.startswith("POSTS|"):
        try:
            _, thread_text, page_text = command.split("|", 2)
            thread_id = int(thread_text); page = max(1, int(page_text))
        except (ValueError, TypeError):
            send_line(calc, "ERR|POSTS"); return token
        status, data = api_request("GET", f"/threads/{thread_id}/posts?page={page}&limit=10", token=token)
        if status != 200:
            send_line(calc, "ERR|POSTS")
        else:
            posts = data.get("posts", [])
            send_line(calc, f"POST_COUNT|{data.get('total', len(posts))}|{page}")
            for post in posts:
                send_line(calc, f"POST|{post['id']}|{clean_field(post.get('username') or '?',20)}|{clean_field(local_timestamp(post.get('created_at')))}|{int(post.get('like_count',0))}|{1 if post.get('liked') else 0}|{int(post.get('reply_count',0))}|{clean_field(post.get('body'),319)}")
            send_line(calc, "POSTS_END")
    elif command == "FORUMS":
        status, data = api_request("GET", "/forums")
        if status != 200:
            send_line(calc, "ERR|FORUMS")
        else:
            forums = data.get("forums", [])
            send_line(calc, f"FORUM_COUNT|{len(forums)}")
            for forum in forums:
                send_line(calc, f"FORUM|{forum['id']}|{int(forum.get('thread_id') or 0)}|{clean_field(forum.get('name'),31)}|{clean_field(forum.get('author') or 'ForumCE',20)}|{clean_field(local_timestamp(forum.get('last_activity')))}")
            send_line(calc, "FORUMS_END")

    # ----- people / profiles -----
    elif command.startswith("PEOPLE|"):
        try: page = max(1, int(command.split("|",1)[1]))
        except Exception: send_line(calc,"ERR|PEOPLE"); return token
        status,data=api_request("GET",f"/users?page={page}&limit=10&pin_creator=true",token=token)
        if status != 200: send_line(calc,"ERR|PEOPLE")
        else:
            total=int(data.get("total",0)); pages=max(1,(total+9)//10)
            send_line(calc,f"PEOPLE_COUNT|{total}|{page}|{pages}|{int(data.get('database_total',total))}")
            for person in data.get("users",[]): emit_person(calc,person)
            send_line(calc,"PEOPLE_END")
    elif command.startswith("PEOPLEJUMP|"):
        letter=clean_field(command.split("|",1)[1],1)
        status,data=api_request("GET",f"/users/letter/{quote(letter)}?limit=10&pin_creator=true",token=token)
        if status==200 and data.get("found"): send_line(calc,f"JUMP|{int(data['page'])}|{int(data['selected'])}")
        else: send_line(calc,"JUMP|0|0")
    elif command.startswith("PROFILE|"):
        username=command.split("|",1)[1]
        status,data=api_request("GET",f"/users/{quote(username)}",token=token)
        if status==200: emit_profile(calc,data)
        else: send_line(calc,"ERR|PROFILE")
    elif command.startswith("FOLLOWERS|") or command.startswith("FOLLOWING|"):
        try:
            mode,username,page_text=command.split("|",2); page=max(1,int(page_text)); path="followers" if mode=="FOLLOWERS" else "following"
        except Exception: send_line(calc,"ERR|LIST"); return token
        status,data=api_request("GET",f"/users/{quote(username)}/{path}?page={page}&limit=10",token=token)
        if status != 200: send_line(calc,"ERR|LIST")
        else:
            total=int(data.get("total",0)); pages=max(1,(total+9)//10)
            send_line(calc,f"LIST_COUNT|{total}|{page}|{pages}")
            for person in data.get("users",[]): emit_person(calc,person)
            send_line(calc,"LIST_END")
    elif command.startswith("RELJUMP|"):
        try:
            _,mode,username,letter=command.split("|",3); path="followers-letter" if mode=="F" else "following-letter"
        except Exception: send_line(calc,"JUMP|0|0"); return token
        status,data=api_request("GET",f"/users/{quote(username)}/{path}/{quote(letter[:1])}?limit=10",token=token)
        if status==200 and data.get("found"): send_line(calc,f"JUMP|{int(data['page'])}|{int(data['selected'])}")
        else: send_line(calc,"JUMP|0|0")
    elif command.startswith("SEARCH|"):
        query=command.split("|",1)[1]
        status,data=api_request("GET",f"/users/search?q={quote(query)}",token=token)
        if status != 200: send_line(calc,"ERR|SEARCH")
        else:
            users=data.get("users",[])[:8]; send_line(calc,f"SEARCH_COUNT|{len(users)}")
            for person in users: emit_person(calc,person)
            send_line(calc,"SEARCH_END")
    elif command.startswith("FOLLOW|"):
        try:
            _,username,want=command.split("|",2); want_follow=bool(int(want))
            status,_=api_request("POST" if want_follow else "DELETE",f"/users/{quote(username)}/follow",token=token)
            if (want_follow and status in (200,409)) or ((not want_follow) and status==200): send_line(calc,"ACTION_OK")
            else: send_line(calc,f"ACTION_ERR|{status}")
        except Exception: send_line(calc,"ACTION_ERR|FOLLOW")

    # ----- direct messages -----
    elif command.startswith("CONVS|"):
        try: page=max(1,int(command.split("|",1)[1]))
        except Exception: send_line(calc,"ERR|CONVS"); return token
        status,data=api_request("GET",f"/messages?page={page}&limit=6",token=token)
        if status != 200: send_line(calc,"ERR|CONVS")
        else:
            total=int(data.get("total",0)); pages=max(1,(total+5)//6)
            send_line(calc,f"CONV_COUNT|{total}|{page}|{pages}|{int(data.get('unread_total',0))}")
            for cv in data.get("conversations",[]):
                send_line(calc,"CONV|{}|{}|{}|{}|{}|{}|{}|{}".format(
                    int(cv.get("id",0)),int(cv.get("other_id",0)),clean_field(cv.get("username"),20),int(cv.get("unread_count",0)),
                    1 if cv.get("last_mine") else 0,clean_field(local_timestamp(cv.get("created_at"))),clean_field(local_timestamp(cv.get("last_message_at"))),clean_field(cv.get("last_message"),44)))
            send_line(calc,"CONVS_END")
    elif command.startswith("DMS|"):
        try:
            _,username,offset_text=command.split("|",2); offset=max(0,int(offset_text))
        except Exception: send_line(calc,"ERR|DMS"); return token
        status,data=api_request("GET",f"/messages/{quote(username)}/window?offset={offset}&limit=3",token=token)
        if status != 200: send_line(calc,"ERR|DMS")
        else:
            msgs=data.get("messages",[]); send_line(calc,f"DM_COUNT|{int(data.get('total',0))}|{offset}|{clean_field(local_timestamp(data.get('created_at')))}")
            for m in msgs:
                send_line(calc,f"DM|{int(m.get('id',0))}|{int(m.get('sender_id',0))}|{1 if m.get('mine') else 0}|{clean_field(m.get('username'),20)}|{clean_field(local_timestamp(m.get('created_at')))}|{clean_field(m.get('body'),160)}")
            send_line(calc,"DMS_END")
    elif command.startswith("SENDDM|"):
        try:
            _,username,body=command.split("|",2); status,_=api_request("POST",f"/messages/{quote(username)}",token=token,body={"body":body}); action_result(calc,status,(201,))
        except Exception: send_line(calc,"ACTION_ERR|SENDDM")
    elif command.startswith("READDM|"):
        username=command.split("|",1)[1]; status,_=api_request("POST",f"/messages/{quote(username)}/read",token=token); action_result(calc,status,(200,))
    elif command.startswith("DELETEDM|"):
        username=command.split("|",1)[1]; status,_=api_request("DELETE",f"/messages/{quote(username)}",token=token); action_result(calc,status,(200,))

    # ----- notifications -----
    elif command.startswith("NOTIFS|"):
        try:
            page=max(1,int(command.split("|",1)[1]))
        except Exception:
            send_line(calc,"ERR|NOTIFS"); return token
        status,data=api_request("GET",f"/notifications?page={page}&limit=10",token=token)
        if status != 200:
            send_line(calc,"ERR|NOTIFS")
        else:
            total=int(data.get("total",0)); pages=max(1,(total+9)//10)
            send_line(calc,f"NOTIF_COUNT|{total}|{page}|{pages}")
            for item in data.get("notifications",[]):
                send_line(calc,"NOTIF|{}|{}|{}|{}|{}|{}".format(
                    int(item.get("id",0)),
                    1 if item.get("read") else 0,
                    clean_field(item.get("type"),11),
                    clean_field(item.get("actor_username") or "ForumCE",20),
                    clean_field(local_timestamp(item.get("created_at"))),
                    clean_field(item.get("text"),95)))
            send_line(calc,"NOTIFS_END")
    elif command == "NOTIFSREAD":
        status,_=api_request("POST","/notifications/read-all",token=token)
        action_result(calc,status,(200,))

    # ----- settings -----
    elif command.startswith("BIO|"):
        bio=command.split("|",1)[1]; status,_=api_request("PUT","/me/bio",token=token,body={"bio":bio}); action_result(calc,status,(200,))
    elif command == "PREFS":
        status,data=api_request("GET","/notification-preferences",token=token)
        if status==200:
            send_line(calc,"PREFS|{}|{}|{}|{}|{}".format(*(1 if data.get(k) else 0 for k in ("master","messages","replies","forum_activity","new_followers"))))
        else: send_line(calc,"ERR|PREFS")
    elif command.startswith("SETPREFS|"):
        try:
            _,a,b,c,d,e=command.split("|",5); body={"master":bool(int(a)),"messages":bool(int(b)),"replies":bool(int(c)),"forum_activity":bool(int(d)),"new_followers":bool(int(e))}
            status,_=api_request("PUT","/notification-preferences",token=token,body=body); action_result(calc,status,(200,))
        except Exception: send_line(calc,"ACTION_ERR|PREFS")
    elif command == "LOGOUT":
        delete_token(); send_line(calc,"ACTION_OK"); send_line(calc,"AUTH_REQUIRED"); return None
    else:
        send_line(calc, "ERR|UNKNOWN")
    return token


def main():
    print("\n==============================\n       ForumCE Bridge\n==============================\n")
    if not server_online():
        print("ForumCE server: OFFLINE")
        return
    print("ForumCE server: ONLINE")

    token = load_token()
    user = authenticated_user(token) if token else None
    if user is None:
        delete_token(); token = login_interactive()
        if token is None: return
        user = authenticated_user(token)
        if user is None: print("Could not verify account."); return

    print(f"Account: {user['username']}")
    print(f"Role:    {user_role(user)}")
    print("\nBridge will automatically wait for/reconnect to the TI-84. Ctrl+C to stop.\n")

    try:
        while True:
            device = find_calculator()
            if device is None:
                time.sleep(0.5); continue
            try:
                calc = serial.Serial(device, BAUD_RATE, timeout=SERIAL_TIMEOUT, write_timeout=2)
                print(f"Calculator: {device}\nUSB:        CONNECTED")
                time.sleep(0.8); send_line(calc, "READY")
                buffer = ""
                last_health_check = 0.0
                known_online = True
                while True:
                    now = time.monotonic()
                    if now - last_health_check >= 2.0:
                        online_now = server_online()
                        if known_online and not online_now:
                            send_line(calc, "OFFLINE")
                        elif (not known_online) and online_now:
                            send_line(calc, "READY")
                        known_online = online_now
                        last_health_check = now
                    data = calc.read(128)
                    if not data: continue
                    buffer += data.decode("utf-8", errors="replace")
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        command = line.strip("\r")
                        if command: token = handle_command(calc, command, token)
            except (serial.SerialException, OSError) as error:
                print(f"USB disconnected ({error}). Waiting for calculator...")
                try: calc.close()
                except Exception: pass
                time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nStopping ForumCE Bridge...")


if __name__ == "__main__":
    main()
