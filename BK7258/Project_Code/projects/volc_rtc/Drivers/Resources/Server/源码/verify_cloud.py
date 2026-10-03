"""Check saved credentials without printing them; never builds firmware.

--llm submits a short text question through the configured RTC model.
--rtc creates an isolated test task, requests a greeting, then stops the task.
These checks verify APIs, not the board's microphone or speaker.
"""
import argparse
import json
import time
import urllib.error
import urllib.request

import server


def verify_llm(settings):
    # No direct third-party/Qwen requests or fallback credentials.
    verify_rtc(settings, ask=True)


def verify_rtc(settings, ask=False):
    request = {"audio_codec": "OPUS", "room_identifier": "VERIFY_",
               "defer_welcome": True, "enable_burst": True,
               "burst_buffer_size": 500, "burst_interval": 20}
    room = server.create_room(settings, request)
    identity = {"AppId": room["app_id"], "RoomId": room["room_id"], "TaskId": room["task_id"]}
    started = False
    try:
        status, reply = server.call_rtc_api(settings, "StartVoiceChat",
                                           server.build_start_body(settings, room, request))
        error = server._api_error(reply, status)
        if error:
            raise RuntimeError("StartVoiceChat failed: " + error)
        started = True
        print("StartVoiceChat: accepted (isolated test room, no board connection)")
        time.sleep(1)
        body = dict(identity, Command="ExternalTextToSpeech", InterruptMode=1,
                    Message="我是小琴，语音通道测试。")
        status, reply = server.call_rtc_api(settings, "UpdateVoiceChat", body)
        error = server._api_error(reply, status)
        if error:
            raise RuntimeError("UpdateVoiceChat failed: " + error)
        print("UpdateVoiceChat: greeting accepted; device playback not tested")
        if ask:
            time.sleep(1)
            body = dict(identity, Command="ExternalTextToLLM", InterruptMode=1,
                        Message="请用一句中文介绍你自己。")
            status, reply = server.call_rtc_api(settings, "UpdateVoiceChat", body)
            error = server._api_error(reply, status)
            if error:
                raise RuntimeError("ExternalTextToLLM failed: " + error)
            print("ExternalTextToLLM: accepted by RTC; generated reply/audio NOT verified")
            time.sleep(2)
    finally:
        if started:
            status, reply = server.call_rtc_api(settings, "StopVoiceChat", identity)
            error = server._api_error(reply, status)
            if error:
                raise RuntimeError("StopVoiceChat failed for test task " + room["task_id"] + ": " + error)
            print("StopVoiceChat: test task stopped")


def verify_local(settings):
    """Exercise the already-running Windows service, not just its cloud helper."""
    base_url = f"http://127.0.0.1:{settings['port']}"

    def post(path, body):
        request = urllib.request.Request(
            base_url + path, data=json.dumps(body).encode("utf-8"),
            headers={"Content-Type": "application/json",
                     "Authorization": "af78e30" + settings["rtc_app_id"]})
        with urllib.request.urlopen(request, timeout=40) as response:
            result = json.load(response)
        if result.get("code") != 200:
            raise RuntimeError(path + " failed: " + str(result.get("msg")))
        return result.get("data", {})

    with urllib.request.urlopen(base_url + "/health", timeout=5) as response:
        health = json.load(response)
    if health.get("data", {}).get("wake_protocol") != 1:
        raise RuntimeError("Running service is outdated; restart it first.")

    room = None
    try:
        room = post("/startvoicechat", {"audio_codec": "OPUS",
                    "room_identifier": "LOCALVERIFY_", "defer_welcome": True})
        identity = {k: room[k] for k in ("app_id", "room_id", "task_id")}
        print("Local /startvoicechat: accepted (isolated test room)")
        time.sleep(1)
        wake = dict(identity, wake_sequence=1)
        post("/wakeupvoicechat", wake)
        post("/wakeupvoicechat", wake)
        print("Local /wakeupvoicechat: accepted, duplicate request accepted without replay")
    finally:
        if room:
            post("/stopvoicechat", {k: room[k] for k in ("app_id", "room_id", "task_id")})
            print("Local /stopvoicechat: test task stopped; board audio not tested")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llm", action="store_true")
    parser.add_argument("--rtc", action="store_true")
    parser.add_argument("--local", action="store_true",
                        help="test the running localhost service using a temporary cloud task")
    args = parser.parse_args()
    settings = server.load_settings()
    if args.llm:
        verify_llm(settings)
    if args.rtc and not args.llm:
        verify_rtc(settings)
    if args.local:
        verify_local(settings)
    if not args.llm and not args.rtc and not args.local:
        print("Configuration parsed; no cloud request made.")
