#!/usr/bin/env python3
"""火山方舟配置管家客户端。

本模块只负责生成脱敏/授权后的配置快照并请求方舟模型。所有本地工具
均由 consumer_gui.py 在用户确认后执行，模型本身不能直接修改文件。
"""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any


ARK_CHAT_URL = "https://ark.cn-beijing.volces.com/api/v3/chat/completions"
DEFAULT_ARK_MODEL = "doubao-seed-2-0-lite-260215"


class AiAssistantError(RuntimeError):
    pass


def _is_sensitive_key(key: str) -> bool:
    normalized = key.lower().replace("_", "")
    return any(
        marker in normalized
        for marker in ("secret", "password", "apikey", "appkey", "token", "accesskey")
    )


def redact_value(value: Any, include_secrets: bool) -> Any:
    """递归处理配置；未授权时不把任何疑似密钥发给模型。"""
    if isinstance(value, dict):
        result: dict[str, Any] = {}
        for key, item in value.items():
            if _is_sensitive_key(str(key)) and not include_secrets:
                result[str(key)] = "<已配置，未授权读取>" if item else "<未配置>"
            else:
                result[str(key)] = redact_value(item, include_secrets)
        return result
    if isinstance(value, list):
        return [redact_value(item, include_secrets) for item in value]
    if isinstance(value, str) and not include_secrets:
        stripped = value.strip()
        if stripped.startswith(("{", "[")):
            try:
                parsed = json.loads(stripped)
            except json.JSONDecodeError:
                return value
            return json.dumps(redact_value(parsed, False), ensure_ascii=False)
    return value


def _secret_strings(value: Any) -> list[str]:
    found: list[str] = []
    if isinstance(value, dict):
        for key, item in value.items():
            if _is_sensitive_key(str(key)) and isinstance(item, (str, int, float)):
                text = str(item)
                if text:
                    found.append(text)
            else:
                found.extend(_secret_strings(item))
    elif isinstance(value, list):
        for item in value:
            found.extend(_secret_strings(item))
    elif isinstance(value, str):
        stripped = value.strip()
        if stripped.startswith(("{", "[")):
            try:
                found.extend(_secret_strings(json.loads(stripped)))
            except json.JSONDecodeError:
                pass
    return found


def build_snapshot(
    config: dict[str, Any],
    template: dict[str, Any],
    logs: str,
    include_secrets: bool,
) -> dict[str, Any]:
    safe_logs = logs[-12000:]
    if not include_secrets:
        for secret in _secret_strings(config) + _secret_strings(template):
            safe_logs = safe_logs.replace(secret, "<密钥已隐藏>")
    return {
        "secret_access": "本次会话已授权" if include_secrets else "未授权，已脱敏",
        "local_config": redact_value(config, include_secrets),
        "voice_agent_template": redact_value(template, include_secrets),
        "recent_logs": safe_logs,
    }


TOOLS: list[dict[str, Any]] = [
    {
        "type": "function",
        "function": {
            "name": "validate_configuration",
            "description": "检查本机配置、智能体模板和服务状态，不修改任何内容。",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "navigate_page",
            "description": "切换桌面端页面，帮助用户完成操作。",
            "parameters": {
                "type": "object",
                "properties": {
                    "page": {
                        "type": "string",
                        "enum": ["home", "setup", "logs", "advanced"],
                    }
                },
                "required": ["page"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "open_official_page",
            "description": "在浏览器打开火山引擎官方配置页面。",
            "parameters": {
                "type": "object",
                "properties": {
                    "page": {
                        "type": "string",
                        "enum": ["ark_api_key", "rtc", "rtc_app", "agent", "docs"],
                    }
                },
                "required": ["page"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "save_network_settings",
            "description": "修改本地服务网络参数。该操作执行前必须由用户确认。",
            "parameters": {
                "type": "object",
                "properties": {
                    "bind_host": {"type": "string"},
                    "port": {"type": "integer", "minimum": 1, "maximum": 65535},
                    "rtc_api_version": {"type": "string"},
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "start_local_service",
            "description": "启动小琴本地服务。执行前必须由用户确认。",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "stop_local_service",
            "description": "停止小琴本地服务。执行前必须由用户确认。",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
]


SYSTEM_PROMPT = """你是“小琴配置管家”，负责帮助普通用户配置本地桌面服务、火山实时音视频和AI智能体。
你可以阅读附带的本地配置快照与最近日志，并在合适时调用允许的工具。
要求：
1. 始终用简短、自然的中文回答，一次只推进最必要的下一步。
2. 即使已获得明文密钥，也绝对不能在回答中复述、输出、比较或截取任何密钥。
3. 不要要求用户在聊天输入框里粘贴密钥，应引导其使用桌面端的专用密钥输入框。
4. 不得虚构执行结果；需要操作时调用工具。写配置、启停服务最终由桌面端再次向用户确认。
5. 优先根据日志给出具体结论，不要给泛泛的排查清单。
"""


def request_plan(
    api_key: str,
    model: str,
    user_text: str,
    snapshot: dict[str, Any],
    history: list[dict[str, str]] | None = None,
    timeout: float = 45.0,
) -> tuple[str, list[dict[str, Any]]]:
    if not api_key.strip():
        raise AiAssistantError("请先填写火山方舟 API Key")
    messages: list[dict[str, Any]] = [
        {
            "role": "system",
            "content": SYSTEM_PROMPT + "\n当前本地状态：\n" + json.dumps(
                snapshot, ensure_ascii=False, separators=(",", ":")
            ),
        }
    ]
    for item in (history or [])[-8:]:
        if item.get("role") in {"user", "assistant"} and item.get("content"):
            messages.append({"role": item["role"], "content": item["content"]})
    messages.append({"role": "user", "content": user_text})
    body = json.dumps(
        {
            "model": model.strip() or DEFAULT_ARK_MODEL,
            "messages": messages,
            "tools": TOOLS,
            "tool_choice": "auto",
            "temperature": 0.2,
            "stream": False,
        },
        ensure_ascii=False,
    ).encode("utf-8")
    request = urllib.request.Request(
        ARK_CHAT_URL,
        data=body,
        headers={
            "Authorization": f"Bearer {api_key.strip()}",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = json.loads(response.read().decode("utf-8", errors="replace"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        try:
            parsed = json.loads(detail)
            detail = parsed.get("error", {}).get("message", detail)
        except (ValueError, TypeError):
            pass
        raise AiAssistantError(f"AI 服务请求失败（HTTP {exc.code}）：{detail}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise AiAssistantError(f"无法连接火山方舟：{exc}") from exc
    except json.JSONDecodeError as exc:
        raise AiAssistantError("AI 服务返回了无法解析的数据") from exc

    try:
        message = payload["choices"][0]["message"]
    except (KeyError, IndexError, TypeError) as exc:
        raise AiAssistantError("AI 服务响应中缺少回答内容") from exc
    content = str(message.get("content") or "").strip()
    actions: list[dict[str, Any]] = []
    for call in message.get("tool_calls") or []:
        function = call.get("function", {})
        name = str(function.get("name", ""))
        try:
            arguments = json.loads(function.get("arguments") or "{}")
        except json.JSONDecodeError:
            arguments = {}
        if name:
            actions.append({"name": name, "arguments": arguments})
    return content, actions
