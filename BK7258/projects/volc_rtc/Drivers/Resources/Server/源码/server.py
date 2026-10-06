#!/usr/bin/env python3
"""Project_Chen 笔记本本地智能体服务。

开发板通过 Windows 移动热点访问本服务；本服务负责生成
RTC Token，并以火山引擎 AK/SK 签名调用 Start/Stop/UpdateVoiceChat。
只使用 Python 标准库，不需安装第三方包。
"""

from __future__ import annotations

import base64
import copy
import datetime as dt
import hashlib
import hmac
import json
import os
import secrets
import struct
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


BASE_DIR = Path(__file__).resolve().parent
PRIVATE_DIR = BASE_DIR / "private"
CONFIG_PATH = PRIVATE_DIR / "server_config.json"
TEMPLATE_PATH = PRIVATE_DIR / "start_voice_chat_template.json"

RTC_API_HOST = "rtc.volcengineapi.com"
RTC_API_REGION = "cn-north-1"
RTC_API_SERVICE = "rtc"
MAX_REQUEST_BYTES = 64 * 1024

PRIV_PUBLISH_STREAM = 0
PRIV_PUBLISH_AUDIO_STREAM = 1
PRIV_PUBLISH_VIDEO_STREAM = 2
PRIV_PUBLISH_DATA_STREAM = 3
PRIV_SUBSCRIBE_STREAM = 4


class ConfigurationError(RuntimeError):
    pass


def _pack_uint16(value: int) -> bytes:
    return struct.pack("<H", int(value))


def _pack_uint32(value: int) -> bytes:
    return struct.pack("<I", int(value))


def _pack_bytes(value: bytes) -> bytes:
    return _pack_uint16(len(value)) + value


def _pack_string(value: str) -> bytes:
    return _pack_bytes(value.encode("utf-8"))


def _pack_privileges(values: dict[int, int]) -> bytes:
    ordered = OrderedDict(sorted(values.items(), key=lambda item: int(item[0])))
    result = _pack_uint16(len(ordered))
    for key, value in ordered.items():
        result += _pack_uint16(key) + _pack_uint32(value)
    return result


def create_rtc_token(app_id: str, app_key: str, room_id: str, user_id: str) -> str:
    """Generate the RTC token format used by the official VolcEngine demo."""
    issued_at = int(time.time())
    expire_at = issued_at + 48 * 60 * 60
    nonce = secrets.randbelow(99_999_999) + 1
    privileges = {
        PRIV_PUBLISH_STREAM: expire_at,
        PRIV_PUBLISH_AUDIO_STREAM: expire_at,
        PRIV_PUBLISH_VIDEO_STREAM: expire_at,
        PRIV_PUBLISH_DATA_STREAM: expire_at,
        PRIV_SUBSCRIBE_STREAM: expire_at,
    }
    message = (
        _pack_uint32(nonce)
        + _pack_uint32(issued_at)
        + _pack_uint32(expire_at)
        + _pack_string(room_id)
        + _pack_string(user_id)
        + _pack_privileges(privileges)
    )
    signature = hmac.new(app_key.encode("utf-8"), message, hashlib.sha256).digest()
    content = _pack_bytes(message) + _pack_bytes(signature)
    return "001" + app_id + base64.b64encode(content).decode("ascii")


def _read_json_document(path: Path) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8-sig").strip()
    except FileNotFoundError as exc:
        raise ConfigurationError(f"缺少文件：{path}") from exc

    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        # 允许直接粘贴控制台复制出来的 POST/curl 示例，提取其中 JSON。
        first = text.find("{")
        last = text.rfind("}")
        if first < 0 or last <= first:
            raise ConfigurationError(f"文件不是有效 JSON：{path}")
        try:
            value = json.loads(text[first : last + 1])
        except json.JSONDecodeError as exc:
            raise ConfigurationError(f"文件不是有效 JSON：{path}（{exc}）") from exc

    if not isinstance(value, dict):
        raise ConfigurationError(f"JSON 顶层必须是对象：{path}")
    return value


def _required_text(config: dict[str, Any], key: str) -> str:
    env_name = {
        "volc_access_key_id": "VOLC_ACCESS_KEY_ID",
        "volc_secret_access_key": "VOLC_SECRET_ACCESS_KEY",
        "rtc_app_id": "VOLC_RTC_APP_ID",
        "rtc_app_key": "VOLC_RTC_APP_KEY",
    }[key]
    value = str(os.environ.get(env_name, config.get(key, ""))).strip()
    if not value or "请填写" in value:
        raise ConfigurationError(f"未配置 {key}（环境变量 {env_name}）")
    return value


def load_settings() -> dict[str, Any]:
    config = _read_json_document(CONFIG_PATH)
    settings = {
        "volc_access_key_id": _required_text(config, "volc_access_key_id"),
        "volc_secret_access_key": _required_text(config, "volc_secret_access_key"),
        "rtc_app_id": _required_text(config, "rtc_app_id"),
        "rtc_app_key": _required_text(config, "rtc_app_key"),
        "rtc_api_version": str(config.get("rtc_api_version", "2025-06-01")).strip(),
        "bind_host": str(config.get("bind_host", "0.0.0.0")).strip(),
        "port": int(config.get("port", 8080)),
    }
    if len(settings["rtc_app_id"]) != 24:
        raise ConfigurationError("rtc_app_id 长度异常，请检查是否复制完整")
    if not 1 <= settings["port"] <= 65535:
        raise ConfigurationError("port 必须在 1~65535 之间")

    template = _read_json_document(TEMPLATE_PATH)
    if "说明" in template.get("Config", {}):
        raise ConfigurationError("请先用小琴智能体的‘代码示例’替换模板内容")
    validate_voice_template(template)
    settings["start_template"] = template
    return settings


def validate_voice_template(template: dict[str, Any]) -> None:
    config = template.get("Config", {})
    llm = config.get("LLMConfig", {})
    if "qwen" in str(llm.get("ModelName", "")).lower():
        raise ConfigurationError("本项目已停用千问，请导入豆包配置；不会自动退回千问。")
    if llm.get("AutoActive") is False:
        raise ConfigurationError("LLMConfig.AutoActive=false 会关闭自动回答，请改为 true。")
    if config.get("TTSConfig", {}).get("AutoActive") is False:
        raise ConfigurationError("TTSConfig.AutoActive=false 会关闭回复的语音合成，请改为 true。")


def voice_profile(settings: dict[str, Any]) -> dict[str, Any]:
    """Only non-secret, actually loaded settings may appear in health/logs."""
    template = settings["start_template"]
    config = template.get("Config", {})
    llm = config.get("LLMConfig", {})
    tts = config.get("TTSConfig", {})
    params = tts.get("ProviderParams", {})
    raw = params.get("VolcanoTTSParameters", "{}")
    try:
        speaker = json.loads(raw).get("req_params", {}).get("speaker", "")
    except (TypeError, ValueError):
        speaker = ""
    return {
        "llm_mode": llm.get("Mode", ""),
        "model": llm.get("ModelName", ""),
        "llm_active": llm.get("AutoActive", True),
        "tts_active": tts.get("AutoActive", True),
        "speaker": speaker,
        "asr_stream_mode": config.get("ASRConfig", {}).get("ProviderParams", {}).get("StreamMode", 0),
    }


def _sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _hmac_sha256(key: bytes, text: str) -> bytes:
    return hmac.new(key, text.encode("utf-8"), hashlib.sha256).digest()


def call_rtc_api(
    settings: dict[str, Any], action: str, body: dict[str, Any]
) -> tuple[int, dict[str, Any]]:
    """Sign and call a VolcEngine RTC OpenAPI request without requests."""
    body_bytes = json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    now = dt.datetime.now(dt.timezone.utc)
    x_date = now.strftime("%Y%m%dT%H%M%SZ")
    content_hash = _sha256_hex(body_bytes)
    content_type = "application/json"
    signed_headers_vector = (
        ("content-type", content_type),
        ("host", RTC_API_HOST),
        ("x-content-sha256", content_hash),
        ("x-date", x_date),
    )
    canonical_headers = "\n".join(":".join(item) for item in signed_headers_vector) + "\n"
    signed_headers = ";".join(item[0] for item in signed_headers_vector)
    query = f"Action={action}&Version={settings['rtc_api_version']}"
    canonical_request = (
        "POST\n/\n"
        + query
        + "\n"
        + canonical_headers
        + "\n"
        + signed_headers
        + "\n"
        + content_hash
    )
    scope = f"{x_date[:8]}/{RTC_API_REGION}/{RTC_API_SERVICE}/request"
    string_to_sign = "HMAC-SHA256\n" + x_date + "\n" + scope + "\n" + _sha256_hex(
        canonical_request.encode("utf-8")
    )
    signature = settings["volc_secret_access_key"].encode("utf-8")
    for item in scope.split("/") + [string_to_sign]:
        signature = _hmac_sha256(signature, item)
    authorization = (
        f"HMAC-SHA256 Credential={settings['volc_access_key_id']}/{scope}, "
        f"SignedHeaders={signed_headers}, Signature={signature.hex()}"
    )
    request = urllib.request.Request(
        f"https://{RTC_API_HOST}/?{query}",
        data=body_bytes,
        method="POST",
        headers={
            "Content-Type": content_type,
            "Host": RTC_API_HOST,
            "X-Content-Sha256": content_hash,
            "X-Date": x_date,
            "Authorization": authorization,
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            raw = response.read().decode("utf-8", errors="replace")
            return response.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8", errors="replace")
        try:
            payload = json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            payload = {"message": raw or str(exc)}
        return exc.code, payload
    except (urllib.error.URLError, TimeoutError) as exc:
        return 503, {"message": f"连接火山引擎失败：{exc}"}


def _api_error(payload: dict[str, Any], status: int) -> str | None:
    metadata = payload.get("ResponseMetadata")
    if isinstance(metadata, dict):
        error = metadata.get("Error")
        if isinstance(error, dict):
            return str(error.get("Message") or error.get("Code") or error)
    if status != 200:
        return str(payload.get("message") or payload)
    result = payload.get("Result")
    if result not in (None, "ok", "OK", True):
        return str(result)
    return None


def _safe_identifier(value: Any, max_length: int = 32) -> str:
    text = "".join(ch for ch in str(value or "") if ch.isalnum() or ch in "_-")
    return text[:max_length]


def create_room(settings: dict[str, Any], device_request: dict[str, Any]) -> dict[str, str]:
    codec = str(device_request.get("audio_codec", "OPUS")).upper()
    if codec not in {"OPUS", "G711A", "G722", "AAC"}:
        codec = "OPUS"
    room_suffix = uuid.uuid4().hex
    room_id = codec + _safe_identifier(device_request.get("room_identifier")) + room_suffix
    user_id = "user" + _safe_identifier(device_request.get("uid_identifier")) + room_suffix
    bot_user_id = "bot" + _safe_identifier(device_request.get("bot_identifier")) + room_suffix
    return {
        "room_id": room_id,
        "uid": user_id,
        "app_id": settings["rtc_app_id"],
        "token": create_rtc_token(
            settings["rtc_app_id"], settings["rtc_app_key"], room_id, user_id
        ),
        "task_id": room_suffix,
        "bot_uid": bot_user_id,
    }


def build_start_body(
    settings: dict[str, Any], room: dict[str, str], device_request: dict[str, Any]
) -> dict[str, Any]:
    body = copy.deepcopy(settings["start_template"])
    body["AppId"] = room["app_id"]
    body["RoomId"] = room["room_id"]
    body["TaskId"] = room["task_id"]

    agent = body.setdefault("AgentConfig", {})
    if not isinstance(agent, dict):
        raise ConfigurationError("智能体模板中 AgentConfig 必须是对象")
    agent["TargetUserId"] = [room["uid"]]
    agent["UserId"] = room["bot_uid"]
    # The board discards remote audio while asleep. New firmware requests the
    # greeting only after its local wake prompt has finished playing.
    if device_request.get("defer_welcome") is True:
        agent["WelcomeMessage"] = ""
    if "enable_burst" in device_request:
        burst = agent.setdefault("Burst", {})
        burst["Enable"] = bool(device_request["enable_burst"])
        burst["BufferSize"] = int(device_request.get("burst_buffer_size", 500))
        burst["Interval"] = int(device_request.get("burst_interval", 20))

    config = body.get("Config")
    if not isinstance(config, dict):
        raise ConfigurationError("智能体模板缺少有效的 Config 对象")
    subtitle = config.get("SubtitleConfig")
    if isinstance(subtitle, dict) and "disable_rts_subtitle" in device_request:
        subtitle["DisableRTSSubtitle"] = bool(device_request["disable_rts_subtitle"])
    return body


COMMAND_MAP = {
    "interrupt": "interrupt",
    "function": "function",
    "external_text_to_speech": "ExternalTextToSpeech",
    "external_prompts_for_llm": "ExternalPromptsForLLM",
    "external_text_to_llm": "ExternalTextToLLM",
    "finish_speech_recognition": "FinishSpeechRecognition",
}


class AgentRequestHandler(BaseHTTPRequestHandler):
    server_version = "ProjectChenLocalAgent/1.0"

    @property
    def settings(self) -> dict[str, Any]:
        return self.server.settings  # type: ignore[attr-defined]

    def log_message(self, format_text: str, *args: Any) -> None:
        print(f"[{self.log_date_time_string()}] {self.client_address[0]} {format_text % args}")

    def _respond(self, status: int, message: str = "", data: Any = None) -> None:
        payload: dict[str, Any] = {"code": status, "msg": message}
        if data is not None:
            payload["data"] = data
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path != "/health":
            self._respond(404, "路径不存在")
            return
        self._respond(200, "本地智能体服务正常", {
            "app_id": self.settings["rtc_app_id"],
            "wake_protocol": 1,
            **voice_profile(self.settings),
        })

    def do_POST(self) -> None:
        try:
            request_body = self._read_device_request()
            if request_body is None:
                return
            if self.path == "/startvoicechat":
                self._start_voice_chat(request_body)
            elif self.path == "/stopvoicechat":
                self._stop_voice_chat(request_body)
            elif self.path == "/updatevoicechat":
                self._update_voice_chat(request_body)
            elif self.path == "/wakeupvoicechat":
                self._wakeup_voice_chat(request_body)
            else:
                self._respond(404, "路径不存在")
        except ConfigurationError as exc:
            self._respond(500, str(exc))
        except (TypeError, ValueError, KeyError) as exc:
            self._respond(400, f"请求参数错误：{exc}")
        except Exception as exc:  # Keep a useful log without exposing credentials.
            print(f"未处理异常：{type(exc).__name__}: {exc}", file=sys.stderr)
            self._respond(500, "本地服务内部错误")

    def _read_device_request(self) -> dict[str, Any] | None:
        content_type = self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if content_type != "application/json":
            self._respond(400, "Content-Type 必须是 application/json")
            return None
        expected_auth = "af78e30" + self.settings["rtc_app_id"]
        if not hmac.compare_digest(self.headers.get("Authorization", ""), expected_auth):
            self._respond(401, "Authorization 校验失败")
            return None
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._respond(400, "Content-Length 无效")
            return None
        if length <= 0 or length > MAX_REQUEST_BYTES:
            self._respond(400, "请求体大小无效")
            return None
        try:
            value = json.loads(self.rfile.read(length).decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._respond(400, "请求体不是有效 JSON")
            return None
        if not isinstance(value, dict):
            self._respond(400, "JSON 顶层必须是对象")
            return None
        return value

    def _start_voice_chat(self, request_body: dict[str, Any]) -> None:
        room = create_room(self.settings, request_body)
        api_body = build_start_body(self.settings, room, request_body)
        status, response = call_rtc_api(self.settings, "StartVoiceChat", api_body)
        error = _api_error(response, status)
        if error:
            print(f"StartVoiceChat 失败（HTTP {status}）：{error}", file=sys.stderr)
            self._respond(502, error)
            return
        print(f"智能体启动请求已接受：room={room['room_id']} task={room['task_id']} "
              f"model={voice_profile(self.settings)['model']}")
        self._respond(200, "", room)

    def _validate_room_fields(self, request_body: dict[str, Any]) -> None:
        for key in ("app_id", "room_id", "task_id"):
            if not str(request_body.get(key, "")).strip():
                raise ValueError(f"缺少 {key}")
        if request_body["app_id"] != self.settings["rtc_app_id"]:
            raise ValueError("app_id 与本地配置不一致")

    def _stop_voice_chat(self, request_body: dict[str, Any]) -> None:
        self._validate_room_fields(request_body)
        api_body = {
            "AppId": request_body["app_id"],
            "RoomId": request_body["room_id"],
            "TaskId": request_body["task_id"],
        }
        status, response = call_rtc_api(self.settings, "StopVoiceChat", api_body)
        error = _api_error(response, status)
        if error:
            self._respond(502, error)
            return
        print(f"智能体已停止：room={request_body['room_id']}")
        self._respond(200, "", request_body)

    def _update_voice_chat(self, request_body: dict[str, Any]) -> None:
        self._validate_room_fields(request_body)
        command = str(request_body.get("command", ""))
        if command not in COMMAND_MAP:
            raise ValueError(f"不支持的 command：{command}")
        api_body: dict[str, Any] = {
            "AppId": request_body["app_id"],
            "RoomId": request_body["room_id"],
            "TaskId": request_body["task_id"],
            "Command": COMMAND_MAP[command],
        }
        if "message" in request_body:
            api_body["Message"] = request_body["message"]
        if "interrupt_mode" in request_body:
            api_body["InterruptMode"] = int(request_body["interrupt_mode"])
        status, response = call_rtc_api(self.settings, "UpdateVoiceChat", api_body)
        error = _api_error(response, status)
        if error:
            print(f"UpdateVoiceChat 失败（HTTP {status}）：{error}", file=sys.stderr)
            self._respond(502, error)
            return
        self._respond(200, "", request_body)

    def _wakeup_voice_chat(self, request_body: dict[str, Any]) -> None:
        self._validate_room_fields(request_body)
        sequence = int(request_body.get("wake_sequence", 0))
        if sequence <= 0:
            raise ValueError("wake_sequence 必须为正整数")
        welcome = str(self.settings["start_template"].get(
            "AgentConfig", {}).get("WelcomeMessage", "")).strip()
        if not welcome:
            raise ConfigurationError("智能体模板没有配置 WelcomeMessage")
        key = (request_body["room_id"], request_body["task_id"], sequence)
        # Serialize duplicate requests so a retry cannot play the greeting twice.
        with self.server.wake_lock:
            now = time.monotonic()
            self.server.completed_wakes = {
                k: t for k, t in self.server.completed_wakes.items() if now - t < 600
            }
            if key in self.server.completed_wakes:
                self._respond(200, "本次唤醒已处理", {"wake_sequence": sequence})
                return
            api_body = {
                "AppId": request_body["app_id"],
                "RoomId": request_body["room_id"],
                "TaskId": request_body["task_id"],
                "Command": "ExternalTextToSpeech",
                "Message": welcome,
                "InterruptMode": 1,
            }
            status, response = call_rtc_api(self.settings, "UpdateVoiceChat", api_body)
            error = _api_error(response, status)
            if error:
                print(f"唤醒欢迎语请求失败（HTTP {status}）：{error}", file=sys.stderr)
                self._respond(502, error)
                return
            self.server.completed_wakes[key] = now
        print(f"唤醒欢迎语已请求：room={request_body['room_id']} 序号={sequence}；等待云端音频")
        self._respond(200, "欢迎语请求已接受", {"wake_sequence": sequence})


class LocalAgentServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address: tuple[str, int], settings: dict[str, Any]):
        super().__init__(address, AgentRequestHandler)
        self.settings = settings
        self.wake_lock = threading.Lock()
        self.completed_wakes: dict[tuple[str, str, int], float] = {}


def main() -> int:
    try:
        settings = load_settings()
    except (ConfigurationError, ValueError) as exc:
        print(f"配置错误：{exc}", file=sys.stderr)
        print("请先双击 configure_local_server.cmd 完成私密配置。", file=sys.stderr)
        return 2

    server = LocalAgentServer((settings["bind_host"], settings["port"]), settings)
    print("Project_Chen 本地智能体服务已启动")
    print(f"监听地址：{settings['bind_host']}:{settings['port']}")
    print("开发板访问：http://192.168.137.1:8080")
    profile = voice_profile(settings)
    print(f"当前模型：{profile['llm_mode']} / {profile['model']}")
    print(f"回复语音合成：{profile['tts_active']}；音色：{profile['speaker']}")
    print("网页修改不会自动同步；导入新模板后请重启服务并重启开发板。")
    print("按 Ctrl+C 停止服务")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n正在停止服务……")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
