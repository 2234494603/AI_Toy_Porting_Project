#!/usr/bin/env python3
"""小琴桌面服务：面向最终用户的本地智能体控制中心。

只依赖 Python 标准库。核心 RTC/OpenAPI 协议继续由 server.py 提供，
本模块负责首次设置、一键启停、状态展示和日志查看。
"""

from __future__ import annotations

import json
import math
import os
import queue
import subprocess
import sys
import threading
import urllib.error
import urllib.request
import webbrowser
from pathlib import Path
from tkinter import (
    BooleanVar,
    Button,
    Canvas,
    Entry,
    Frame,
    Label,
    PhotoImage,
    StringVar,
    Text,
    Tk,
    messagebox,
)
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText
from typing import Any, Callable

from ai_config_assistant import (
    AiAssistantError,
    DEFAULT_ARK_MODEL,
    build_snapshot,
    request_plan,
)


BASE_DIR = Path(__file__).resolve().parent
PRIVATE_DIR = BASE_DIR / "private"
CONFIG_PATH = PRIVATE_DIR / "server_config.json"
TEMPLATE_PATH = PRIVATE_DIR / "start_voice_chat_template.json"
SERVER_SCRIPT = BASE_DIR / "server.py"

APP_ID_DEFAULT = "6ab7750fd755df017a8abe2f"
API_VERSION_DEFAULT = "2025-06-01"
PORT_DEFAULT = 8080
BOARD_URL = "http://192.168.137.1:8080"

URL_ACCESS_KEY = "https://console.volcengine.com/iam/keymanage"
URL_RTC_APP = (
    "https://console.volcengine.com/rtc/listRTC/appConfig?appId=" + APP_ID_DEFAULT
)
URL_AGENT = "https://console.volcengine.com/conversational-ai/agentManage"
URL_ARK_API_KEY = "https://ark.volcengine.com/region:cn-beijing/apikey"
URL_RTC_HOME = "https://console.volcengine.com/rtc"
URL_AI_DOCS = (
    "https://docs.volcengine.com/docs/real_time_communication/"
    "StartAIconversationStartVoiceChat?lang=zh&redirect=1"
)

# Apple 风格的系统中性色与单一蓝色强调，品牌色仅用于状态反馈。
NAVY = "#1D1D1F"
NAVY_2 = "#DCEBFA"
SLATE = "#86868B"
PALE_BLUE = "#EAF4FF"
GOLD = "#0071E3"
GOLD_DARK = "#005BB5"
GOLD_SOFT = "#F0F7FF"
RED = "#FF3B30"
RED_SOFT = "#FFF0EF"
MAGENTA = "#0071E3"
MAGENTA_SOFT = "#EAF4FF"
GREEN = "#248A3D"
GREEN_SOFT = "#ECF8EF"
INK = "#1D1D1F"
MUTED = "#6E6E73"
SURFACE = "#FCFDFE"
BACKGROUND = "#F2F5F9"
BORDER = "#DCE3EB"

FONT = "Segoe UI Variable"
FONT_MONO = "Consolas"
LOG_LIMIT = 4000


def _smooth_circle_coverage(
    x: float, y: float, center_x: float, center_y: float, radius: float
) -> float:
    distance = math.hypot(x - center_x, y - center_y) - radius
    return max(0.0, min(1.0, 0.5 - distance))


def _smooth_brand_image(
    master,
    width: int,
    height: int,
    background: str,
    *,
    compact: bool,
) -> PhotoImage:
    """生成与玻璃按钮一致的抗锯齿品牌图形，避免 Canvas 圆弧毛边。"""
    red, green, blue = master.winfo_rgb(background)
    backdrop = (red // 257, green // 257, blue // 257)
    accent_red, accent_green, accent_blue = master.winfo_rgb(GOLD)
    accent = (accent_red // 257, accent_green // 257, accent_blue // 257)
    pale_red, pale_green, pale_blue = master.winfo_rgb(PALE_BLUE)
    pale = (pale_red // 257, pale_green // 257, pale_blue // 257)
    white = (255, 255, 255)
    center_x = width / 2.0 if compact else width - 64.0
    center_y = height / 2.0
    outer_radius = min(width, height) / 2.0 - 2.0 if compact else 64.0
    ring_radius = outer_radius * (0.60 if compact else 0.70)
    ring_width = 3.2 if compact else 11.5
    dot_radius = 3.6 if compact else 11.5
    pixels = bytearray()

    def blend(
        first: tuple[int, int, int],
        second: tuple[int, int, int],
        amount: float,
    ) -> tuple[int, int, int]:
        amount = max(0.0, min(1.0, amount))
        return tuple(
            round(left * (1.0 - amount) + right * amount)
            for left, right in zip(first, second)
        )

    for py in range(height):
        for px in range(width):
            x, y = px + 0.5, py + 0.5
            color = backdrop
            outer = _smooth_circle_coverage(
                x, y, center_x, center_y, outer_radius
            )
            outer_color = accent if compact else pale
            if outer > 0.0:
                reflection = max(0.0, 1.0 - py / max(height * 0.56, 1.0))
                outer_color = blend(outer_color, white, reflection * 0.10)
                color = blend(color, outer_color, outer)

            radial = math.hypot(x - center_x, y - center_y)
            ring_distance = abs(radial - ring_radius) - ring_width / 2.0
            ring = max(0.0, min(1.0, 0.5 - ring_distance))
            angle = math.degrees(math.atan2(-(y - center_y), x - center_x))
            gap_edge = (abs(angle) - 48.0) / 2.0 + 0.5
            ring *= max(0.0, min(1.0, gap_edge))
            if ring > 0.0:
                color = blend(color, white if compact else accent, ring)

            dot = _smooth_circle_coverage(
                x, y, center_x, center_y, dot_radius
            )
            if dot > 0.0:
                color = blend(color, white if compact else accent, dot)
            pixels.extend(color)

    ppm = f"P6 {width} {height} 255\n".encode("ascii") + bytes(pixels)
    return PhotoImage(master=master, data=ppm, format="PPM")


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("JSON 顶层必须是对象")
    return value


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    try:
        os.chmod(path, 0o600)
    except OSError:
        pass


def extract_json(text: str) -> dict[str, Any]:
    """接受纯 JSON 或火山控制台复制出的 POST/curl 代码示例。"""
    text = text.strip()
    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        first = text.find("{")
        last = text.rfind("}")
        if first < 0 or last <= first:
            raise ValueError("剪贴板中没有找到 JSON")
        try:
            value = json.loads(text[first : last + 1])
        except json.JSONDecodeError as exc:
            raise ValueError(f"代码示例中的 JSON 无法解析：{exc}") from exc
    if not isinstance(value, dict) or not isinstance(value.get("Config"), dict):
        raise ValueError("JSON 中缺少 Config，请确认复制的是智能体代码示例")
    return value


def template_summary(template: dict[str, Any]) -> tuple[str, str, str]:
    config = template.get("Config", {})
    llm = config.get("LLMConfig", {})
    tts = config.get("TTSConfig", {})
    raw = tts.get("ProviderParams", {}).get("VolcanoTTSParameters", "{}")
    try:
        speaker = json.loads(raw).get("req_params", {}).get("speaker", "")
    except (TypeError, ValueError, json.JSONDecodeError):
        speaker = ""
    return (
        str(llm.get("ModelName", "未识别")),
        "已开启" if tts.get("AutoActive", True) else "未开启",
        speaker or "未识别",
    )


def validate_template(template: dict[str, Any]) -> None:
    config = template.get("Config", {})
    if "说明" in config:
        raise ValueError("仍是示例占位模板，请导入小琴的代码示例")
    llm = config.get("LLMConfig", {})
    if "qwen" in str(llm.get("ModelName", "")).lower():
        raise ValueError("当前项目不使用千问，请导入火山豆包配置")
    if llm.get("AutoActive") is False:
        raise ValueError("大模型自动回答被关闭")
    if config.get("TTSConfig", {}).get("AutoActive") is False:
        raise ValueError("回复语音合成被关闭")


def private_ready() -> bool:
    try:
        config = read_json(CONFIG_PATH)
        required = (
            "volc_access_key_id",
            "volc_secret_access_key",
            "rtc_app_id",
            "rtc_app_key",
        )
        values = [str(config.get(key, "")).strip() for key in required]
        if any(not value or "请填写" in value for value in values):
            return False
        template = read_json(TEMPLATE_PATH)
        validate_template(template)
        return True
    except (OSError, ValueError, json.JSONDecodeError):
        return False


def health_request(port: int) -> tuple[int, dict[str, Any] | None, str | None]:
    try:
        with urllib.request.urlopen(
            f"http://127.0.0.1:{port}/health", timeout=1.3
        ) as response:
            raw = response.read().decode("utf-8", errors="replace")
            return response.status, json.loads(raw), None
    except (urllib.error.URLError, TimeoutError, OSError, json.JSONDecodeError) as exc:
        return 0, None, str(exc)


class RoundedButton(Canvas):
    """带抗锯齿、玻璃高光和完整交互状态的圆角按钮。"""

    def __init__(
        self,
        parent,
        text: str,
        command: Callable[[], None],
        *,
        width: int = 148,
        height: int = 42,
        radius: int = 15,
        fill: str = GOLD,
        foreground: str = NAVY,
        hover: str | None = None,
        disabled_fill: str = "#E7EBF1",
        disabled_foreground: str = "#9AA6B5",
        pressed: str | None = None,
        font: tuple[Any, ...] = (FONT, 9, "bold"),
    ):
        super().__init__(
            parent,
            width=width,
            height=height,
            bg=parent.cget("bg"),
            highlightthickness=0,
            bd=0,
            cursor="hand2",
            takefocus=1,
        )
        self._text = text
        self._command = command
        self._radius = radius
        self._fill = fill
        self._foreground = foreground
        self._hover = hover or fill
        self._pressed_fill = pressed or self._hover
        self._disabled_fill = disabled_fill
        self._disabled_foreground = disabled_foreground
        self._font = font
        self._enabled = True
        self._hovering = False
        self._pressed = False
        self._focused = False
        self._glass_cache: dict[tuple[Any, ...], PhotoImage] = {}
        self._glass_image: PhotoImage | None = None
        self.bind("<Configure>", lambda _event: self._draw())
        self.bind("<Enter>", self._on_enter)
        self.bind("<Leave>", self._on_leave)
        self.bind("<ButtonPress-1>", self._on_press)
        self.bind("<ButtonRelease-1>", self._on_click)
        self.bind("<FocusIn>", self._on_focus_in)
        self.bind("<FocusOut>", self._on_focus_out)
        self.bind("<KeyPress-space>", self._on_key_press)
        self.bind("<KeyRelease-space>", self._on_key_release)
        self.bind("<KeyPress-Return>", self._on_key_press)
        self.bind("<KeyRelease-Return>", self._on_key_release)
        self.after_idle(self._draw)

    def _rgb(self, color: str) -> tuple[int, int, int]:
        red, green, blue = self.winfo_rgb(color)
        return red // 257, green // 257, blue // 257

    @staticmethod
    def _mix(
        first: tuple[int, int, int],
        second: tuple[int, int, int],
        amount: float,
    ) -> tuple[int, int, int]:
        amount = max(0.0, min(1.0, amount))
        return tuple(
            round(left * (1.0 - amount) + right * amount)
            for left, right in zip(first, second)
        )

    @staticmethod
    def _rounded_coverage(
        x: float,
        y: float,
        width: int,
        height: int,
        inset: float,
        radius: float,
    ) -> float:
        """用有符号距离计算 1px 抗锯齿覆盖率，消除圆角拼接毛刺。"""
        half_width = max(1.0, (width - inset * 2.0) / 2.0)
        half_height = max(1.0, (height - inset * 2.0) / 2.0)
        radius = max(1.0, min(radius, half_width, half_height))
        qx = abs(x - width / 2.0) - (half_width - radius)
        qy = abs(y - height / 2.0) - (half_height - radius)
        outside = (max(qx, 0.0) ** 2 + max(qy, 0.0) ** 2) ** 0.5
        distance = outside + min(max(qx, qy), 0.0) - radius
        return max(0.0, min(1.0, 0.5 - distance))

    def _render_glass(self, width: int, height: int, fill: str,
                      state: str) -> PhotoImage:
        background = str(self.cget("bg"))
        cache_key = (
            width, height, self._radius, fill, background, state, self._focused
        )
        cached = self._glass_cache.get(cache_key)
        if cached is not None:
            return cached

        base = self._rgb(fill)
        backdrop = self._rgb(background)
        white = (255, 255, 255)
        shadow = (26, 36, 52)
        focus = self._rgb(GOLD)
        pixels = bytearray()
        inset = 2.2 if self._focused else 1.35
        radius = float(self._radius)

        for py in range(height):
            vertical = py / max(height - 1, 1)
            for px in range(width):
                x = px + 0.5
                y = py + 0.5

                # 柔和的下投影只负责分离层级，不画硬边。
                shadow_alpha = self._rounded_coverage(
                    x, y - 1.1, width, height, inset + 0.5, radius
                ) * 0.13
                color = self._mix(backdrop, shadow, shadow_alpha)

                outer = self._rounded_coverage(
                    x, y, width, height, inset, radius
                )
                inner = self._rounded_coverage(
                    x, y, width, height, inset + 0.9, max(1.0, radius - 0.9)
                )

                if self._focused:
                    focus_outer = self._rounded_coverage(
                        x, y, width, height, 0.45, radius + 1.8
                    )
                    ring = max(0.0, focus_outer - outer)
                    color = self._mix(color, focus, ring * 0.72)

                if outer > 0.0:
                    glass = base
                    if state == "pressed":
                        glass = self._mix(glass, shadow, 0.10)
                        reflection = 0.025
                    else:
                        if state == "hover":
                            glass = self._mix(glass, white, 0.055)
                        reflection = max(0.0, 1.0 - vertical / 0.48) * 0.17

                    # 上半部是一层连续反射，不使用会产生锯齿的独立图形。
                    glass = self._mix(glass, white, reflection)
                    if vertical > 0.62:
                        glass = self._mix(
                            glass, shadow, (vertical - 0.62) / 0.38 * 0.045
                        )

                    # 细玻璃边缘：顶部偏亮、底部轻微收暗。
                    edge = max(0.0, outer - inner)
                    edge_color = white if vertical < 0.58 else self._mix(base, shadow, 0.13)
                    glass = self._mix(glass, edge_color, edge * 0.56)
                    color = self._mix(color, glass, outer)

                pixels.extend(color)

        ppm = f"P6 {width} {height} 255\n".encode("ascii") + bytes(pixels)
        image = PhotoImage(master=self, data=ppm, format="PPM")
        self._glass_cache[cache_key] = image
        return image

    def _draw(self) -> None:
        self.delete("all")
        width = max(self.winfo_width(), int(self.cget("width")))
        height = max(self.winfo_height(), int(self.cget("height")))
        if not self._enabled:
            fill, foreground = self._disabled_fill, self._disabled_foreground
            state = "disabled"
        else:
            fill = self._pressed_fill if self._pressed else (
                self._hover if self._hovering else self._fill
            )
            foreground = self._foreground
            state = "pressed" if self._pressed else (
                "hover" if self._hovering else "default"
            )
        self._glass_image = self._render_glass(width, height, fill, state)
        self.create_image(0, 0, anchor="nw", image=self._glass_image)
        self.create_text(
            width / 2,
            height / 2 + (1 if self._pressed else 0),
            text=self._text,
            fill=foreground,
            font=self._font,
        )

    def _on_enter(self, _event) -> None:
        if self._enabled:
            self._hovering = True
            self._draw()

    def _on_leave(self, _event) -> None:
        self._hovering = False
        self._pressed = False
        self._draw()

    def _on_press(self, _event) -> None:
        if self._enabled:
            self.focus_set()
            self._pressed = True
            self._draw()

    def _on_click(self, _event) -> None:
        self._pressed = False
        self._draw()
        if self._enabled:
            self._command()

    def _on_focus_in(self, _event) -> None:
        self._focused = True
        self._draw()

    def _on_focus_out(self, _event) -> None:
        self._focused = False
        self._pressed = False
        self._draw()

    def _on_key_press(self, _event) -> str:
        if self._enabled:
            self._pressed = True
            self._draw()
        return "break"

    def _on_key_release(self, _event) -> str:
        was_pressed = self._pressed
        self._pressed = False
        self._draw()
        if self._enabled and was_pressed:
            self._command()
        return "break"

    def set_enabled(self, enabled: bool) -> None:
        self._enabled = enabled
        self.configure(cursor="hand2" if enabled else "arrow")
        self._draw()

    def set_text(self, text: str) -> None:
        self._text = text
        self._draw()


class ConsumerApp:
    def __init__(self, root: Tk, initial_page: str = "home", autostart: bool = False):
        self.root = root
        self.root.title("小琴桌面服务")
        self.root.geometry("1180x675+40+40")
        self.root.minsize(980, 620)
        self.root.configure(bg=BACKGROUND)

        self.proc: subprocess.Popen[str] | None = None
        self.log_queue: queue.Queue[str] = queue.Queue()
        self.closing = False
        self.health_busy = False
        self.service_reachable = False
        self.device_seen = False
        self.ai_busy = False
        self.ai_secret_authorized = False
        self.ai_history: list[dict[str, str]] = []
        self.pages: dict[str, Frame] = {}
        self.nav_buttons: dict[str, Button] = {}
        self.secret_entries: list[Entry] = []

        self._configure_styles()
        self._build_shell()
        self._load_config_fields()
        self._load_template_editor()
        self.show_page(initial_page)
        self._set_service_state("未启动", "服务目前处于关闭状态", "off")
        self._schedule_logs()
        self._schedule_health()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        if not private_ready():
            self._set_notice("完成首次设置后即可启动服务", "warn")
        elif autostart:
            self.root.after(350, self.start_service)

    def _configure_styles(self) -> None:
        style = ttk.Style(self.root)
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure(
            "Consumer.TCheckbutton",
            background=SURFACE,
            foreground=MUTED,
            font=(FONT, 9),
        )
        style.configure(
            "Consumer.Vertical.TScrollbar",
            background=PALE_BLUE,
            troughcolor=SURFACE,
            bordercolor=SURFACE,
            arrowcolor=NAVY,
        )

    def _build_shell(self) -> None:
        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(1, weight=1)

        sidebar = Frame(self.root, bg="#FAFAFC", width=210,
                        highlightbackground=BORDER, highlightthickness=1)
        sidebar.grid(row=0, column=0, sticky="nsew")
        sidebar.grid_propagate(False)
        sidebar.grid_rowconfigure(8, weight=1)

        brand = Frame(sidebar, bg="#FAFAFC")
        brand.grid(row=0, column=0, sticky="ew", padx=24, pady=(30, 28))
        mark = Canvas(brand, width=42, height=42, bg="#FAFAFC", highlightthickness=0)
        mark.pack(side="left")
        self.brand_mark_image = _smooth_brand_image(
            mark, 42, 42, "#FAFAFC", compact=True
        )
        mark.create_image(0, 0, anchor="nw", image=self.brand_mark_image)
        text = Frame(brand, bg="#FAFAFC")
        text.pack(side="left", padx=(10, 0))
        Label(text, text="小琴", bg="#FAFAFC", fg=NAVY, font=(FONT, 18, "bold")).pack(anchor="w")
        Label(text, text="桌面服务", bg="#FAFAFC", fg=MUTED, font=(FONT, 8)).pack(anchor="w")

        items = (
            ("home", "首页", "运行、接入与 AI 配置"),
            ("setup", "手动设置", "账号与智能体"),
            ("logs", "运行记录", "连接与错误信息"),
            ("advanced", "高级设置", "网络与原始模板"),
        )
        for row, (key, title, subtitle) in enumerate(items, start=1):
            btn = Button(
                sidebar,
                text=f"{title}\n{subtitle}",
                command=lambda name=key: self.show_page(name),
                bg="#FAFAFC",
                fg=MUTED,
                activebackground=PALE_BLUE,
                activeforeground=NAVY,
                relief="flat",
                bd=0,
                anchor="w",
                justify="left",
                padx=24,
                pady=11,
                font=(FONT, 10),
                cursor="hand2",
            )
            btn.grid(row=row, column=0, sticky="ew", pady=2)
            self.nav_buttons[key] = btn

        palette = Canvas(sidebar, height=4, bg="#FAFAFC", highlightthickness=0)
        palette.grid(row=9, column=0, sticky="ew", padx=24, pady=(0, 12))
        palette.bind("<Configure>", self._draw_palette)
        Label(
            sidebar,
            text="本地运行 · 密钥仅保存在此电脑",
            bg="#FAFAFC",
            fg="#94A3B8",
            font=(FONT, 8),
        ).grid(row=10, column=0, sticky="w", padx=24, pady=(0, 24))

        main = Frame(self.root, bg=BACKGROUND)
        main.grid(row=0, column=1, sticky="nsew")
        main.grid_rowconfigure(1, weight=1)
        main.grid_columnconfigure(0, weight=1)

        header = Frame(main, bg=BACKGROUND)
        header.grid(row=0, column=0, sticky="ew", padx=34, pady=(24, 12))
        header.grid_columnconfigure(0, weight=1)
        self.page_title = StringVar(value="首页")
        Label(header, textvariable=self.page_title, bg=BACKGROUND, fg=INK,
              font=(FONT, 19, "bold")).grid(row=0, column=0, sticky="w")
        self.header_status = Label(
            header, text="● 未启动", bg=RED_SOFT, fg=RED,
            font=(FONT, 9, "bold"), padx=13, pady=7,
        )
        self.header_status.grid(row=0, column=1, sticky="e")

        self.container = Frame(main, bg=BACKGROUND)
        self.container.grid(row=1, column=0, sticky="nsew", padx=34, pady=(0, 28))
        self.container.grid_rowconfigure(0, weight=1)
        self.container.grid_columnconfigure(0, weight=1)

        self.pages["home"] = self._build_home()
        self.pages["setup"] = self._build_setup()
        self.pages["logs"] = self._build_logs()
        self.pages["advanced"] = self._build_advanced()
        for page in self.pages.values():
            page.grid(row=0, column=0, sticky="nsew")

    def _draw_palette(self, event) -> None:
        canvas: Canvas = event.widget
        canvas.delete("all")
        width = max(event.width, 4)
        colors = (SLATE, GOLD, RED, MAGENTA)
        for index, color in enumerate(colors):
            canvas.create_rectangle(
                index * width / 4, 0, (index + 1) * width / 4, 6,
                fill=color, outline="",
            )

    def _page(self) -> Frame:
        return Frame(self.container, bg=BACKGROUND)

    def _card(self, parent, **grid) -> Frame:
        card = Frame(parent, bg=SURFACE, highlightbackground=BORDER, highlightthickness=1)
        card.grid(**grid)
        return card

    def _primary_button(self, parent, text: str,
                        command: Callable[[], None]) -> RoundedButton:
        return RoundedButton(
            parent, text=text, command=command, width=166, height=44,
            radius=14, fill=GOLD, foreground="white", hover="#0077ED",
            pressed=GOLD_DARK,
            font=(FONT, 10, "bold"),
        )

    def _secondary_button(self, parent, text: str,
                          command: Callable[[], None]) -> RoundedButton:
        return RoundedButton(
            parent, text=text, command=command, width=138, height=40,
            radius=13, fill="#E8E8ED", foreground=INK, hover="#DEDEE3",
            pressed="#D2D2D7",
            font=(FONT, 9, "bold"),
        )

    def _build_home(self) -> Frame:
        page = self._page()
        page.grid_columnconfigure(0, weight=3, uniform="home-columns")
        page.grid_columnconfigure(1, weight=2, uniform="home-columns")
        page.grid_rowconfigure(1, weight=1)

        hero = Frame(page, bg=SURFACE, height=150,
                     highlightbackground=BORDER, highlightthickness=1)
        hero.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 14))
        hero.grid_propagate(False)
        hero.grid_columnconfigure(0, weight=1)
        hero_text = Frame(hero, bg=SURFACE)
        hero_text.grid(row=0, column=0, sticky="nsw", padx=26, pady=21)
        self.service_title = StringVar(value="小琴尚未启动")
        Label(hero_text, textvariable=self.service_title, bg=SURFACE, fg=INK,
              font=(FONT, 24, "bold")).pack(anchor="w", pady=(0, 3))
        self.service_detail = StringVar(value="点击启动后，开发板即可连接本机服务")
        Label(hero_text, textvariable=self.service_detail, bg=SURFACE, fg=MUTED,
              font=(FONT, 10)).pack(anchor="w")
        actions = Frame(hero_text, bg=SURFACE)
        actions.pack(anchor="w", pady=(15, 0))
        self.start_button = self._primary_button(actions, "启动小琴服务", self.start_service)
        self.start_button.pack(side="left")
        self.stop_button = RoundedButton(
            actions, text="停止", command=self.stop_service, width=86, height=44,
            radius=14, fill="#E8E8ED", foreground=RED, hover="#DEDEE3",
            pressed="#D2D2D7",
            font=(FONT, 10, "bold"),
        )
        self.stop_button.set_enabled(False)
        self.stop_button.pack(side="left", padx=(10, 0))
        accent = Canvas(hero, width=196, height=148, bg=SURFACE, highlightthickness=0)
        accent.grid(row=0, column=1, sticky="nse")
        self.hero_mark_image = _smooth_brand_image(
            accent, 196, 148, SURFACE, compact=False
        )
        accent.create_image(0, 0, anchor="nw", image=self.hero_mark_image)

        assistant = self._card(page, row=1, column=0, sticky="nsew", padx=(0, 7))
        assistant.grid_rowconfigure(4, weight=1)
        assistant.grid_columnconfigure(0, weight=1)
        top = Frame(assistant, bg=SURFACE)
        top.grid(row=0, column=0, sticky="ew", padx=22, pady=(18, 8))
        Label(top, text="AI 自动配置", bg=SURFACE, fg=INK,
              font=(FONT, 14, "bold")).pack(side="left")
        self.ai_secret_status = StringVar(value="默认隐藏密钥")
        Label(top, textvariable=self.ai_secret_status, bg=PALE_BLUE, fg=GOLD_DARK,
              font=(FONT, 8, "bold"), padx=9, pady=5).pack(side="right")
        Label(
            assistant,
            text="告诉小琴你的目标，它会检查配置和日志；实际修改前仍由你确认。",
            bg=SURFACE, fg=MUTED, font=(FONT, 9),
        ).grid(row=1, column=0, sticky="w", padx=22)

        self.ai_vars = {
            "ark_api_key": StringVar(),
            "ark_model": StringVar(value=DEFAULT_ARK_MODEL),
        }
        connect = Frame(assistant, bg=SURFACE)
        connect.grid(row=2, column=0, sticky="ew", padx=22, pady=(12, 8))
        connect.grid_columnconfigure(0, weight=1)
        Label(connect, text="方舟 API Key", bg=SURFACE, fg=MUTED,
              font=(FONT, 8, "bold")).grid(
                  row=0, column=0, columnspan=3, sticky="w", pady=(0, 5)
              )
        self.ai_key_entry = Entry(
            connect, textvariable=self.ai_vars["ark_api_key"], show="●",
            bg="#F5F5F7", fg=INK, insertbackground=INK, relief="flat", bd=0,
            highlightbackground=BORDER, highlightcolor=GOLD, highlightthickness=1,
            font=(FONT, 9),
        )
        self.ai_key_entry.grid(row=1, column=0, sticky="ew", ipady=9)
        self.ai_key_entry.insert(0, "")
        self._secondary_button(connect, "保存 AI Key", self._save_ai_settings).grid(
            row=1, column=1, padx=(9, 0)
        )
        self.ai_secret_button = RoundedButton(
            connect, text="允许读取密钥", command=self._toggle_ai_secret_access,
            width=126, height=40, radius=13, fill="#E8E8ED", foreground=INK,
            hover="#DEDEE3", pressed="#D2D2D7", font=(FONT, 8, "bold"),
        )
        self.ai_secret_button.grid(row=1, column=2, padx=(9, 0))

        self.ai_chat = ScrolledText(
            assistant, width=1, height=5, bg="#F9F9FB", fg=INK, relief="flat", bd=0,
            highlightbackground=BORDER, highlightthickness=1,
            font=(FONT, 9), padx=13, pady=10, wrap="word", state="disabled",
        )
        self.ai_chat.grid(row=4, column=0, sticky="nsew", padx=22)
        self.ai_chat.tag_configure("assistant", foreground=INK, spacing1=4, spacing3=8)
        self.ai_chat.tag_configure("user", foreground=GOLD_DARK, spacing1=4, spacing3=8)
        self.ai_chat.tag_configure("system", foreground=MUTED, spacing1=4, spacing3=8)
        compose = Frame(assistant, bg=SURFACE)
        compose.grid(row=5, column=0, sticky="ew", padx=22, pady=(10, 18))
        compose.grid_columnconfigure(0, weight=1)
        self.ai_input = Text(
            compose, width=1, height=2, bg="#F5F5F7", fg=INK, insertbackground=INK,
            relief="flat", bd=0, highlightbackground=BORDER,
            highlightcolor=GOLD, highlightthickness=1, font=(FONT, 9), wrap="word",
        )
        self.ai_input.grid(row=0, column=0, sticky="ew", padx=(0, 9))
        self.ai_send_button = RoundedButton(
            compose, text="发送", command=self._send_ai_message,
            width=84, height=44, radius=14, fill=GOLD, foreground="white",
            hover="#0077ED", pressed=GOLD_DARK, font=(FONT, 9, "bold"),
        )
        self.ai_send_button.grid(row=0, column=1, sticky="ns")
        self._ai_append(
            "AI 管家",
            "我可以直接检查当前配置。你也可以先点击右侧“自动检查”。",
            "assistant",
        )

        side = Frame(page, bg=BACKGROUND)
        side.grid(row=1, column=1, sticky="nsew", padx=(7, 0))
        side.grid_columnconfigure(0, weight=1)
        side.grid_rowconfigure(1, weight=1)

        status_card = Frame(side, bg=SURFACE, highlightbackground=BORDER, highlightthickness=1)
        status_card.grid(row=0, column=0, sticky="ew", pady=(0, 12))
        status_card.grid_columnconfigure(0, weight=1)
        Label(status_card, text="当前状态", bg=SURFACE, fg=INK,
              font=(FONT, 12, "bold")).grid(row=0, column=0, sticky="w", padx=18, pady=(13, 7))
        self.step_labels: list[Label] = []
        steps = (
            ("电脑服务", "等待启动"),
            ("开发板", "等待连接热点"),
            ("语音对话", "等待智能体上线"),
        )
        for index, (title, subtitle) in enumerate(steps, start=1):
            row = Frame(status_card, bg=SURFACE)
            row.grid(row=index, column=0, sticky="ew", padx=18, pady=2)
            row.grid_columnconfigure(1, weight=1)
            badge = Label(row, text="●", width=2, bg=SURFACE, fg="#C7C7CC",
                          font=(FONT, 9, "bold"), pady=3)
            badge.grid(row=0, column=0, sticky="w")
            Label(row, text=title, bg=SURFACE, fg=INK,
                  font=(FONT, 9, "bold")).grid(row=0, column=1, sticky="w", padx=(7, 4))
            state = Label(row, text=subtitle, bg=SURFACE, fg=MUTED, font=(FONT, 8))
            state.grid(row=0, column=2, sticky="e")
            self.step_labels.append(state)
        self.notice = Label(status_card, text="", bg=GOLD_SOFT, fg=GOLD_DARK,
                            font=(FONT, 8), anchor="w", padx=10, pady=7)
        self.notice.grid(row=4, column=0, sticky="ew", padx=18, pady=(7, 12))

        guide = Frame(side, bg=SURFACE, highlightbackground=BORDER, highlightthickness=1)
        guide.grid(row=1, column=0, sticky="nsew")
        guide.grid_columnconfigure(0, weight=1)
        guide.grid_columnconfigure(1, weight=1)
        Label(guide, text="快速接入", bg=SURFACE, fg=INK,
              font=(FONT, 12, "bold")).grid(
                  row=0, column=0, columnspan=2, sticky="w", padx=18, pady=(13, 3)
              )
        Label(guide, text="常用入口集中在这里，无需来回切换页面。",
              bg=SURFACE, fg=MUTED, font=(FONT, 8)).grid(
                  row=1, column=0, columnspan=2, sticky="w", padx=18, pady=(0, 8)
              )
        quick_actions = (
            ("自动检查当前配置", self._ask_ai_to_configure),
            ("实时音视频控制台", lambda: webbrowser.open(URL_RTC_HOME)),
            ("AI 应用与 AppKey", lambda: webbrowser.open(URL_RTC_APP)),
            ("小琴智能体管理", lambda: webbrowser.open(URL_AGENT)),
            ("手动填写全部设置", lambda: self.show_page("setup")),
        )
        for index, (text, command) in enumerate(quick_actions):
            if index == 0:
                row, column, columnspan = 2, 0, 2
            else:
                row, column, columnspan = 3 + (index - 1) // 2, (index - 1) % 2, 1
            button = RoundedButton(
                guide, text=text, command=command, width=112, height=30,
                radius=11, fill=PALE_BLUE if index == 0 else "#F2F2F7",
                foreground=GOLD_DARK if index == 0 else INK,
                hover=NAVY_2 if index == 0 else "#E8E8ED",
                pressed="#D2D2D7", font=(FONT, 8, "bold"),
            )
            button.grid(
                row=row, column=column, columnspan=columnspan, sticky="ew",
                padx=(18 if column == 0 else 4, 18 if column + columnspan == 2 else 4),
                pady=2,
            )

        self.address_var = StringVar(value=BOARD_URL)
        self.model_var = StringVar(value="等待服务启动")
        self.voice_var = StringVar(value="俏皮女声")
        device = Frame(guide, bg=SURFACE)
        device.grid(row=5, column=0, columnspan=2, sticky="ew", padx=18, pady=(7, 11))
        device.grid_columnconfigure(0, weight=1)
        Label(device, text="开发板地址", bg=SURFACE, fg=MUTED,
              font=(FONT, 8)).grid(row=0, column=0, sticky="w")
        Label(device, textvariable=self.address_var, bg=SURFACE, fg=INK,
              font=(FONT, 8, "bold")).grid(row=1, column=0, sticky="w", pady=(2, 0))
        RoundedButton(
            device, text="复制", command=self._copy_board_url, width=62, height=30,
            radius=10, fill="#E8E8ED", foreground=INK, hover="#DEDEE3",
            pressed="#D2D2D7", font=(FONT, 8, "bold"),
        ).grid(row=0, column=1, rowspan=2, padx=(8, 0))
        return page

    def _info_row(self, parent, row: int, title: str, variable: StringVar,
                  color: str, foreground: str) -> None:
        box = Frame(parent, bg=color)
        box.grid(row=row, column=0, sticky="ew", padx=22, pady=5)
        Label(box, text=title, bg=color, fg=foreground,
              font=(FONT, 8, "bold")).pack(anchor="w", padx=12, pady=(9, 1))
        Label(box, textvariable=variable, bg=color, fg=INK,
              font=(FONT, 9)).pack(anchor="w", padx=12, pady=(0, 9))

    def _build_assistant(self) -> Frame:
        page = self._page()
        page.grid_columnconfigure(0, weight=2)
        page.grid_columnconfigure(1, weight=5)
        page.grid_rowconfigure(0, weight=1)

        settings = self._card(page, row=0, column=0, sticky="nsew", padx=(0, 8))
        settings.grid_columnconfigure(0, weight=1)
        Label(settings, text="连接豆包", bg=SURFACE, fg=INK,
              font=(FONT, 14, "bold")).grid(row=0, column=0, sticky="w", padx=20, pady=(20, 4))
        Label(
            settings,
            text="AI 管家使用独立的火山方舟 API Key。密钥只保存在本机。",
            bg=SURFACE, fg=MUTED, font=(FONT, 9), wraplength=245, justify="left",
        ).grid(row=1, column=0, sticky="w", padx=20)
        self.ai_vars = {
            "ark_api_key": StringVar(),
            "ark_model": StringVar(value=DEFAULT_ARK_MODEL),
        }
        Label(settings, text="方舟 API Key", bg=SURFACE, fg=INK,
              font=(FONT, 9, "bold")).grid(row=2, column=0, sticky="w", padx=20, pady=(18, 5))
        self.ai_key_entry = Entry(
            settings, textvariable=self.ai_vars["ark_api_key"], show="●",
            bg="#F8FAFD", fg=INK, insertbackground=INK, relief="flat", bd=0,
            highlightbackground=BORDER, highlightcolor=GOLD, highlightthickness=1,
            font=(FONT, 9),
        )
        self.ai_key_entry.grid(row=3, column=0, sticky="ew", padx=20, ipady=8)
        Label(settings, text="模型", bg=SURFACE, fg=INK,
              font=(FONT, 9, "bold")).grid(row=4, column=0, sticky="w", padx=20, pady=(13, 5))
        Entry(
            settings, textvariable=self.ai_vars["ark_model"], bg="#F8FAFD", fg=INK,
            insertbackground=INK, relief="flat", bd=0, highlightbackground=BORDER,
            highlightcolor=GOLD, highlightthickness=1, font=(FONT, 9),
        ).grid(row=5, column=0, sticky="ew", padx=20, ipady=8)
        key_actions = Frame(settings, bg=SURFACE)
        key_actions.grid(row=6, column=0, sticky="ew", padx=20, pady=(12, 4))
        self._link_button(key_actions, "获取方舟 Key", URL_ARK_API_KEY).pack(side="left")
        self._secondary_button(settings, "保存 AI 设置", self._save_ai_settings).grid(
            row=7, column=0, sticky="w", padx=20, pady=(8, 18)
        )

        privacy = Frame(settings, bg=GOLD_SOFT)
        privacy.grid(row=8, column=0, sticky="ew", padx=20, pady=(0, 12))
        self.ai_secret_status = StringVar(value="当前仅向 AI 提供脱敏配置")
        Label(privacy, textvariable=self.ai_secret_status, bg=GOLD_SOFT, fg=GOLD_DARK,
              font=(FONT, 8, "bold"), wraplength=235, justify="left").pack(
                  anchor="w", padx=12, pady=(10, 5)
              )
        Label(
            privacy,
            text="授权后，RTC 与火山密钥会随提问发送给火山方舟；AI 不会在回答中显示密钥。",
            bg=GOLD_SOFT, fg=MUTED, font=(FONT, 8), wraplength=235, justify="left",
        ).pack(anchor="w", padx=12, pady=(0, 10))
        self.ai_secret_button = RoundedButton(
            settings, text="授权读取明文密钥", command=self._toggle_ai_secret_access,
            width=168, height=38, radius=14, fill=RED_SOFT, foreground=RED,
            hover="#FFD9CF", font=(FONT, 8, "bold"),
        )
        self.ai_secret_button.grid(row=9, column=0, sticky="w", padx=20, pady=(0, 18))

        chat = self._card(page, row=0, column=1, sticky="nsew", padx=(8, 0))
        chat.grid_rowconfigure(1, weight=1)
        chat.grid_columnconfigure(0, weight=1)
        top = Frame(chat, bg=SURFACE)
        top.grid(row=0, column=0, sticky="ew", padx=22, pady=(18, 10))
        Label(top, text="AI 配置管家", bg=SURFACE, fg=INK,
              font=(FONT, 14, "bold")).pack(side="left")
        Label(top, text="诊断 · 引导 · 执行前确认", bg=GREEN_SOFT, fg=GREEN,
              font=(FONT, 8, "bold"), padx=9, pady=5).pack(side="right")
        self.ai_chat = ScrolledText(
            chat, bg="#FBFCFE", fg=INK, relief="flat", bd=0,
            highlightbackground=BORDER, highlightthickness=1,
            font=(FONT, 9), padx=14, pady=12, wrap="word", state="disabled",
        )
        self.ai_chat.grid(row=1, column=0, sticky="nsew", padx=22)
        self.ai_chat.tag_configure("assistant", foreground=NAVY, spacing1=5, spacing3=10)
        self.ai_chat.tag_configure("user", foreground=MAGENTA, spacing1=5, spacing3=10)
        self.ai_chat.tag_configure("system", foreground=MUTED, spacing1=5, spacing3=10)
        compose = Frame(chat, bg=SURFACE)
        compose.grid(row=2, column=0, sticky="ew", padx=22, pady=(12, 18))
        compose.grid_columnconfigure(0, weight=1)
        self.ai_input = Text(
            compose, height=3, bg="#F8FAFD", fg=INK, insertbackground=INK,
            relief="flat", bd=0, highlightbackground=BORDER,
            highlightcolor=GOLD, highlightthickness=1, font=(FONT, 9), wrap="word",
        )
        self.ai_input.grid(row=0, column=0, sticky="ew", padx=(0, 10))
        self.ai_send_button = RoundedButton(
            compose, text="发送", command=self._send_ai_message,
            width=92, height=52, radius=17, fill=GOLD, foreground=NAVY,
            hover="#FFD261", font=(FONT, 9, "bold"),
        )
        self.ai_send_button.grid(row=0, column=1, sticky="ns")
        self._ai_append("AI 管家", "你好，我可以检查本机配置、分析日志，并在你确认后执行允许的操作。", "assistant")
        return page

    def _build_setup(self) -> Frame:
        page = self._page()
        page.grid_columnconfigure(0, weight=1)
        page.grid_columnconfigure(1, weight=1)
        page.grid_rowconfigure(0, weight=1)

        form_card = self._card(page, row=0, column=0, sticky="nsew", padx=(0, 8))
        form_card.grid_columnconfigure(0, weight=1)
        Label(form_card, text="连接火山引擎", bg=SURFACE, fg=INK,
              font=(FONT, 15, "bold")).grid(row=0, column=0, sticky="w", padx=24, pady=(22, 4))
        Label(form_card, text="密钥只保存在本机 private 目录，不会显示在运行日志中。",
              bg=SURFACE, fg=MUTED, font=(FONT, 9)).grid(row=1, column=0, sticky="w", padx=24)

        self.config_vars: dict[str, StringVar] = {
            "volc_access_key_id": StringVar(),
            "volc_secret_access_key": StringVar(),
            "rtc_app_id": StringVar(value=APP_ID_DEFAULT),
            "rtc_app_key": StringVar(),
        }
        labels = (
            ("volc_access_key_id", "AccessKey ID", True),
            ("volc_secret_access_key", "SecretAccessKey", True),
            ("rtc_app_id", "AI 应用 AppId", False),
            ("rtc_app_key", "RTC AppKey", True),
        )
        fields = Frame(form_card, bg=SURFACE)
        fields.grid(row=2, column=0, sticky="ew", padx=24, pady=(18, 6))
        fields.grid_columnconfigure(0, weight=1)
        for row, (key, title, secret) in enumerate(labels):
            Label(fields, text=title, bg=SURFACE, fg=INK,
                  font=(FONT, 9, "bold")).grid(row=row * 2, column=0, sticky="w", pady=(5, 4))
            entry = Entry(
                fields, textvariable=self.config_vars[key], bg="#F8FAFD", fg=INK,
                insertbackground=INK, relief="flat", bd=0, font=(FONT, 10),
                highlightbackground=BORDER, highlightcolor=GOLD,
                highlightthickness=1,
            )
            if secret:
                entry.configure(show="●")
                self.secret_entries.append(entry)
            entry.grid(row=row * 2 + 1, column=0, sticky="ew", ipady=9)
        self.show_secret_var = BooleanVar(value=False)
        ttk.Checkbutton(
            form_card, text="显示密钥", variable=self.show_secret_var,
            command=self._toggle_secrets, style="Consumer.TCheckbutton",
        ).grid(row=3, column=0, sticky="w", padx=24, pady=(5, 8))
        links = Frame(form_card, bg=SURFACE)
        links.grid(row=4, column=0, sticky="ew", padx=24)
        self._link_button(links, "获取 AccessKey", URL_ACCESS_KEY).pack(side="left")
        self._link_button(links, "查看 RTC AppKey", URL_RTC_APP).pack(side="left", padx=8)

        import_card = self._card(page, row=0, column=1, sticky="nsew", padx=(8, 0))
        import_card.grid_columnconfigure(0, weight=1)
        Label(import_card, text="导入小琴智能体", bg=SURFACE, fg=INK,
              font=(FONT, 15, "bold")).grid(row=0, column=0, sticky="w", padx=24, pady=(22, 4))
        Label(
            import_card,
            text="在火山控制台打开“小琴”卡片，点击“代码示例”，再点击右下角“复制”。",
            bg=SURFACE, fg=MUTED, font=(FONT, 9), justify="left", wraplength=390,
        ).grid(row=1, column=0, sticky="w", padx=24)
        step_box = Frame(import_card, bg=PALE_BLUE)
        step_box.grid(row=2, column=0, sticky="ew", padx=24, pady=(18, 12))
        for index, text in enumerate(("打开智能体管理", "复制小琴代码示例", "回到这里一键导入"), start=1):
            Label(step_box, text=f"{index}  {text}", bg=PALE_BLUE, fg=NAVY,
                  font=(FONT, 9, "bold")).pack(anchor="w", padx=14, pady=7)
        self.template_state = StringVar(value="尚未导入智能体配置")
        self.template_state_label = Label(
            import_card, textvariable=self.template_state, bg=GOLD_SOFT, fg=GOLD_DARK,
            font=(FONT, 9, "bold"), anchor="w", padx=12, pady=10,
        )
        self.template_state_label.grid(row=3, column=0, sticky="ew", padx=24, pady=6)
        actions = Frame(import_card, bg=SURFACE)
        actions.grid(row=4, column=0, sticky="ew", padx=24, pady=(8, 12))
        self._link_button(actions, "打开智能体管理", URL_AGENT).pack(side="left")
        self._secondary_button(actions, "从剪贴板导入", self._import_template).pack(side="right")
        self._primary_button(import_card, "保存并完成设置", self._save_first_setup).grid(
            row=5, column=0, sticky="ew", padx=24, pady=(14, 22)
        )
        return page

    def _link_button(self, parent, text: str, url: str) -> RoundedButton:
        width = max(112, min(190, 44 + len(text) * 14))
        return RoundedButton(
            parent, text=f"{text}  ↗", command=lambda: webbrowser.open(url),
            width=width, height=36, radius=14, fill=MAGENTA_SOFT,
            foreground=MAGENTA, hover=NAVY_2, pressed="#D2D2D7",
            font=(FONT, 8, "bold"),
        )

    def _build_logs(self) -> Frame:
        page = self._page()
        page.grid_rowconfigure(1, weight=1)
        page.grid_columnconfigure(0, weight=1)
        bar = Frame(page, bg=BACKGROUND)
        bar.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        Label(bar, text="服务运行记录", bg=BACKGROUND, fg=MUTED,
              font=(FONT, 9)).pack(side="left")
        self._secondary_button(bar, "复制", self._copy_logs).pack(side="right")
        self._secondary_button(bar, "清空", self._clear_logs).pack(side="right", padx=8)
        self.log_view = ScrolledText(
            page, bg="#121D31", fg="#DDE7F4", insertbackground="white",
            selectbackground=MAGENTA, relief="flat", bd=0,
            font=(FONT_MONO, 10), padx=16, pady=14, wrap="word",
        )
        self.log_view.grid(row=1, column=0, sticky="nsew")
        self.log_view.tag_configure("gui", foreground="#7EC8E3")
        self.log_view.tag_configure("ok", foreground="#77D9B3")
        self.log_view.tag_configure("warn", foreground="#FFD76A")
        self.log_view.tag_configure("error", foreground="#FF8A7A")
        return page

    def _build_advanced(self) -> Frame:
        page = self._page()
        page.grid_columnconfigure(0, weight=1)
        page.grid_rowconfigure(1, weight=1)
        network = self._card(page, row=0, column=0, sticky="ew", pady=(0, 12))
        Label(network, text="网络参数", bg=SURFACE, fg=INK,
              font=(FONT, 12, "bold")).grid(row=0, column=0, columnspan=6,
                                             sticky="w", padx=20, pady=(16, 10))
        self.advanced_vars = {
            "bind_host": StringVar(value="0.0.0.0"),
            "port": StringVar(value=str(PORT_DEFAULT)),
            "rtc_api_version": StringVar(value=API_VERSION_DEFAULT),
        }
        for column, (key, title) in enumerate((
            ("bind_host", "绑定地址"), ("port", "端口"), ("rtc_api_version", "API 版本")
        )):
            Label(network, text=title, bg=SURFACE, fg=MUTED,
                  font=(FONT, 8, "bold")).grid(row=1, column=column * 2,
                                                sticky="w", padx=(20, 6), pady=(0, 14))
            Entry(network, textvariable=self.advanced_vars[key], width=18,
                  bg="#F8FAFD", fg=INK, relief="flat", bd=0,
                  highlightthickness=1, highlightbackground=BORDER,
                  highlightcolor=GOLD, font=(FONT, 9)).grid(
                      row=1, column=column * 2 + 1, sticky="ew", ipady=7,
                      padx=(0, 14), pady=(0, 14)
                  )
        template = self._card(page, row=1, column=0, sticky="nsew")
        template.grid_rowconfigure(1, weight=1)
        template.grid_columnconfigure(0, weight=1)
        top = Frame(template, bg=SURFACE)
        top.grid(row=0, column=0, sticky="ew", padx=20, pady=(14, 8))
        Label(top, text="智能体原始模板", bg=SURFACE, fg=INK,
              font=(FONT, 12, "bold")).pack(side="left")
        self._secondary_button(top, "格式化", self._format_template).pack(side="right")
        self.template_editor = ScrolledText(
            template, bg="#F8FAFD", fg=INK, insertbackground=INK,
            relief="flat", bd=0, font=(FONT_MONO, 9), wrap="none",
            highlightbackground=BORDER, highlightthickness=1,
        )
        self.template_editor.grid(row=1, column=0, sticky="nsew", padx=20)
        bottom = Frame(template, bg=SURFACE)
        bottom.grid(row=2, column=0, sticky="ew", padx=20, pady=(10, 16))
        Label(bottom, text="普通用户无需修改此处。", bg=SURFACE, fg=MUTED,
              font=(FONT, 8)).pack(side="left")
        self._primary_button(bottom, "保存高级设置", self._save_advanced).pack(side="right")
        return page

    def _build_help(self) -> Frame:
        page = self._page()
        page.grid_columnconfigure(0, weight=1)
        page.grid_columnconfigure(1, weight=1)
        intro = self._card(page, row=0, column=0, columnspan=2, sticky="ew", pady=(0, 12))
        intro.grid_columnconfigure(0, weight=1)
        Label(intro, text="接入小琴，只需要四步", bg=SURFACE, fg=INK,
              font=(FONT, 16, "bold")).pack(anchor="w", padx=24, pady=(20, 5))
        Label(intro, text="每一步都能直接打开对应页面。准备好账号后，通常几分钟就能完成。",
              bg=SURFACE, fg=MUTED, font=(FONT, 10)).pack(anchor="w", padx=24, pady=(0, 10))
        self._link_button(intro, "查看官方接入文档", URL_AI_DOCS).pack(
            anchor="w", padx=24, pady=(0, 20)
        )
        steps = (
            ("01", "开通实时音视频", "进入火山引擎实时音视频控制台，确认服务已经开通。",
             GOLD_SOFT, GOLD_DARK, "打开实时音视频", URL_RTC_HOME, None),
            ("02", "确认 AI 应用", "打开 AI 应用并复制 AppId 与 AppKey，密钥不要发给其他人。",
             PALE_BLUE, NAVY, "查看 AI 应用", URL_RTC_APP, None),
            ("03", "创建小琴智能体", "完成模型、人设和声音设置，再从“代码示例”复制配置。",
             MAGENTA_SOFT, MAGENTA, "打开智能体管理", URL_AGENT, None),
            ("04", "回到本机完成设置", "粘贴账号配置并导入代码示例，保存后即可启动服务。",
             RED_SOFT, RED, "去首次设置", None, lambda: self.show_page("setup")),
        )
        for index, (number, title, body, bg, fg, action, url, command) in enumerate(steps):
            card = self._card(page, row=index // 2 + 1, column=index % 2,
                              sticky="nsew", padx=(0, 6) if index % 2 == 0 else (6, 0), pady=6)
            Label(card, text=number, bg=bg, fg=fg, font=(FONT, 11, "bold"),
                  width=3, pady=7).pack(anchor="w", padx=20, pady=(18, 10))
            Label(card, text=title, bg=SURFACE, fg=INK,
                  font=(FONT, 12, "bold")).pack(anchor="w", padx=20)
            Label(card, text=body, bg=SURFACE, fg=MUTED, font=(FONT, 9),
                  wraplength=380, justify="left").pack(anchor="w", padx=20, pady=(6, 12))
            if url:
                self._link_button(card, action, url).pack(anchor="w", padx=20, pady=(0, 18))
            else:
                RoundedButton(
                    card, text=action, command=command, width=132, height=36,
                    radius=14, fill=RED_SOFT, foreground=RED,
                    hover="#FFD9CF", font=(FONT, 8, "bold"),
                ).pack(anchor="w", padx=20, pady=(0, 18))
        return page

    def show_page(self, name: str) -> None:
        if name not in self.pages:
            name = "home"
        titles = {
            "home": "首页", "setup": "手动设置", "logs": "运行记录",
            "advanced": "高级设置",
        }
        self.page_title.set(titles[name])
        self.pages[name].tkraise()
        for key, button in self.nav_buttons.items():
            button.configure(
                bg=PALE_BLUE if key == name else "#FAFAFC",
                fg=NAVY if key == name else MUTED,
            )

    def _load_config_fields(self) -> None:
        data: dict[str, Any] = {}
        try:
            data = read_json(CONFIG_PATH)
        except (OSError, ValueError, json.JSONDecodeError):
            pass
        for key, var in self.config_vars.items():
            var.set(str(data.get(key, APP_ID_DEFAULT if key == "rtc_app_id" else "")))
        for key, var in self.advanced_vars.items():
            defaults = {"bind_host": "0.0.0.0", "port": PORT_DEFAULT,
                        "rtc_api_version": API_VERSION_DEFAULT}
            var.set(str(data.get(key, defaults[key])))
        for key, var in self.ai_vars.items():
            defaults = {"ark_api_key": "", "ark_model": DEFAULT_ARK_MODEL}
            var.set(str(data.get(key, defaults[key])))

    def _load_template_editor(self) -> None:
        self.template_editor.delete("1.0", "end")
        try:
            template = read_json(TEMPLATE_PATH)
            self.template_editor.insert("1.0", json.dumps(template, ensure_ascii=False, indent=2))
            validate_template(template)
            model, tts, speaker = template_summary(template)
            self.template_state.set(f"已导入 · {model} · 语音{tts}")
            self.template_state_label.configure(bg=GREEN_SOFT, fg=GREEN)
            self.model_var.set(model)
            self.voice_var.set("俏皮女声" if "qiaopinvsheng" in speaker else speaker)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            self.template_state.set(f"尚未完成导入 · {exc}")
            self.template_state_label.configure(bg=GOLD_SOFT, fg=GOLD_DARK)

    def _toggle_secrets(self) -> None:
        marker = "" if self.show_secret_var.get() else "●"
        for entry in self.secret_entries:
            entry.configure(show=marker)

    def _save_ai_settings(self) -> None:
        api_key = self.ai_vars["ark_api_key"].get().strip()
        model = self.ai_vars["ark_model"].get().strip()
        if not api_key:
            messagebox.showwarning("缺少 API Key", "请先填写火山方舟 API Key。", parent=self.root)
            return
        if not model:
            messagebox.showwarning("缺少模型", "请填写豆包模型名称。", parent=self.root)
            return
        try:
            config = read_json(CONFIG_PATH) if CONFIG_PATH.exists() else {}
            config.update({"ark_api_key": api_key, "ark_model": model})
            write_json(CONFIG_PATH, config)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            messagebox.showerror("保存失败", str(exc), parent=self.root)
            return
        self._ai_append("系统", "AI 管家设置已保存在本机。", "system")

    def _toggle_ai_secret_access(self) -> None:
        if self.ai_secret_authorized:
            self.ai_secret_authorized = False
            self.ai_secret_status.set("默认隐藏密钥")
            self.ai_secret_button.set_text("允许读取密钥")
            self._ai_append("系统", "明文密钥授权已撤销。", "system")
            return
        confirmed = messagebox.askyesno(
            "授权 AI 读取明文密钥",
            "授权仅在本次软件运行期间有效。\n\n"
            "授权后，你发送问题时，本机保存的火山 AccessKey、SecretAccessKey、"
            "RTC AppKey、方舟 API Key，以及智能体模板中的密钥字段会以明文发送给"
            "火山方舟模型用于诊断。\n\n"
            "AI 被要求不得在回答中显示密钥；关闭软件或点击撤销后授权立即失效。\n\n"
            "是否继续授权？",
            parent=self.root,
        )
        if not confirmed:
            return
        self.ai_secret_authorized = True
        self.ai_secret_status.set("本次会话已授权")
        self.ai_secret_button.set_text("撤销密钥权限")
        self._ai_append("系统", "已授权本次会话读取明文密钥。", "system")

    def _ai_append(self, speaker: str, text: str, tag: str) -> None:
        self.ai_chat.configure(state="normal")
        self.ai_chat.insert("end", f"{speaker}\n{text.strip()}\n\n", tag)
        self.ai_chat.configure(state="disabled")
        self.ai_chat.see("end")

    def _ask_ai_to_configure(self) -> None:
        prompt = "请检查当前配置和最近日志，直接告诉我缺少什么，并带我完成接入。"
        self.ai_input.delete("1.0", "end")
        self.ai_input.insert("1.0", prompt)
        if not self.ai_vars["ark_api_key"].get().strip():
            self.ai_key_entry.focus_set()
            self._ai_append(
                "系统",
                "先在上方填写一次方舟 API Key，保存后再点“自动检查”。",
                "system",
            )
            return
        self._send_ai_message()

    def _send_ai_message(self) -> None:
        if self.ai_busy:
            return
        prompt = self.ai_input.get("1.0", "end").strip()
        if not prompt:
            return
        api_key = self.ai_vars["ark_api_key"].get().strip()
        model = self.ai_vars["ark_model"].get().strip() or DEFAULT_ARK_MODEL
        if not api_key:
            messagebox.showwarning(
                "需要方舟 API Key",
                "请先在左侧填写并保存火山方舟 API Key。",
                parent=self.root,
            )
            return
        try:
            config = read_json(CONFIG_PATH) if CONFIG_PATH.exists() else {}
        except (OSError, ValueError, json.JSONDecodeError):
            config = {}
        # 输入框里尚未保存的新 Key 也应反映为已配置，但不会写入运行日志。
        config["ark_api_key"] = api_key
        config["ark_model"] = model
        try:
            template = read_json(TEMPLATE_PATH) if TEMPLATE_PATH.exists() else {}
        except (OSError, ValueError, json.JSONDecodeError):
            template = {}
        logs = self.log_view.get("1.0", "end") if hasattr(self, "log_view") else ""
        snapshot = build_snapshot(config, template, logs, self.ai_secret_authorized)
        history = list(self.ai_history)
        self.ai_input.delete("1.0", "end")
        self._ai_append("你", prompt, "user")
        self.ai_busy = True
        self.ai_send_button.set_enabled(False)
        threading.Thread(
            target=self._ai_worker,
            args=(api_key, model, prompt, snapshot, history),
            daemon=True,
        ).start()

    def _ai_worker(self, api_key: str, model: str, prompt: str,
                   snapshot: dict[str, Any], history: list[dict[str, str]]) -> None:
        try:
            content, actions = request_plan(api_key, model, prompt, snapshot, history)
            error = None
        except AiAssistantError as exc:
            content, actions, error = "", [], str(exc)
        except Exception as exc:  # 避免后台线程异常导致按钮永久不可用
            content, actions, error = "", [], f"AI 管家出现异常：{exc}"
        if not self.closing:
            self.root.after(
                0,
                lambda: self._ai_done(prompt, content, actions, error),
            )

    def _ai_done(self, prompt: str, content: str, actions: list[dict[str, Any]],
                 error: str | None) -> None:
        self.ai_busy = False
        self.ai_send_button.set_enabled(True)
        if error:
            self._ai_append("AI 管家", error, "system")
            return
        answer = content or ("我准备执行下面的操作。" if actions else "暂时没有得到有效回答。")
        self._ai_append("AI 管家", answer, "assistant")
        self.ai_history.extend((
            {"role": "user", "content": prompt},
            {"role": "assistant", "content": answer},
        ))
        self.ai_history = self.ai_history[-12:]
        for action in actions:
            result = self._execute_ai_action(action)
            if result:
                self._ai_append("系统", result, "system")

    def _execute_ai_action(self, action: dict[str, Any]) -> str:
        name = str(action.get("name", ""))
        arguments = action.get("arguments", {})
        if not isinstance(arguments, dict):
            arguments = {}
        if name == "validate_configuration":
            parts = ["基础配置完整" if private_ready() else "基础配置尚未完成"]
            parts.append("本地服务在线" if self.service_reachable else "本地服务未在线")
            parts.append("开发板已连接" if self.device_seen else "尚未检测到开发板连接")
            return "配置检查：" + "；".join(parts) + "。"
        if name == "navigate_page":
            page = str(arguments.get("page", "home"))
            if page in self.pages:
                self.show_page(page)
                return f"已切换到“{self.page_title.get()}”。"
            return "AI 请求的页面不存在，未执行。"
        if name == "open_official_page":
            urls = {
                "ark_api_key": URL_ARK_API_KEY,
                "rtc": URL_RTC_HOME,
                "rtc_app": URL_RTC_APP,
                "agent": URL_AGENT,
                "docs": URL_AI_DOCS,
            }
            target = str(arguments.get("page", ""))
            if target not in urls:
                return "AI 请求的官网入口不存在，未执行。"
            webbrowser.open(urls[target])
            return "已在浏览器打开对应的火山引擎官方页面。"
        if name == "save_network_settings":
            changes: list[str] = []
            for key, label in (("bind_host", "绑定地址"), ("port", "端口"),
                               ("rtc_api_version", "API 版本")):
                if key in arguments:
                    changes.append(f"{label}：{arguments[key]}")
            if not changes:
                return "AI 没有提供可保存的网络参数。"
            if not messagebox.askyesno(
                "允许 AI 修改网络设置？",
                "AI 建议修改：\n\n" + "\n".join(changes) + "\n\n是否应用？",
                parent=self.root,
            ):
                return "用户取消了网络设置修改。"
            try:
                config = read_json(CONFIG_PATH) if CONFIG_PATH.exists() else {}
                if "bind_host" in arguments:
                    config["bind_host"] = str(arguments["bind_host"])
                    self.advanced_vars["bind_host"].set(config["bind_host"])
                if "port" in arguments:
                    port = int(arguments["port"])
                    if not 1 <= port <= 65535:
                        raise ValueError("端口必须在 1 到 65535 之间")
                    config["port"] = port
                    self.advanced_vars["port"].set(str(port))
                if "rtc_api_version" in arguments:
                    config["rtc_api_version"] = str(arguments["rtc_api_version"])
                    self.advanced_vars["rtc_api_version"].set(config["rtc_api_version"])
                write_json(CONFIG_PATH, config)
            except (OSError, ValueError, json.JSONDecodeError) as exc:
                return f"网络设置保存失败：{exc}"
            return "网络设置已经保存；服务运行中时需要重启后生效。"
        if name == "start_local_service":
            if not messagebox.askyesno(
                "允许 AI 启动服务？", "AI 建议现在启动小琴本地服务，是否执行？", parent=self.root
            ):
                return "用户取消了启动服务。"
            self.start_service()
            return "已提交启动服务操作。"
        if name == "stop_local_service":
            if not messagebox.askyesno(
                "允许 AI 停止服务？", "停止后开发板会中断当前对话，是否执行？", parent=self.root
            ):
                return "用户取消了停止服务。"
            self.stop_service()
            return "已提交停止服务操作。"
        return f"未识别的 AI 操作：{name}"

    def _import_template(self) -> None:
        try:
            template = extract_json(self.root.clipboard_get())
            validate_template(template)
        except Exception as exc:
            messagebox.showerror("导入失败", str(exc), parent=self.root)
            return
        self.template_editor.delete("1.0", "end")
        self.template_editor.insert("1.0", json.dumps(template, ensure_ascii=False, indent=2))
        model, tts, _speaker = template_summary(template)
        self.template_state.set(f"已读取 · {model} · 语音{tts}")
        self.template_state_label.configure(bg=GREEN_SOFT, fg=GREEN)
        self._append_log("已从剪贴板读取小琴智能体配置。\n", "ok")

    def _collect_config(self) -> dict[str, Any]:
        values = {key: var.get().strip() for key, var in self.config_vars.items()}
        labels = {
            "volc_access_key_id": "AccessKey ID",
            "volc_secret_access_key": "SecretAccessKey",
            "rtc_app_id": "AI 应用 AppId",
            "rtc_app_key": "RTC AppKey",
        }
        for key, label in labels.items():
            if not values[key]:
                raise ValueError(f"请填写 {label}")
            if "请填写" in values[key]:
                raise ValueError(f"{label} 仍是占位内容，请填写真实配置")
        if len(values["rtc_app_id"]) != 24:
            raise ValueError("AI 应用 AppId 应为 24 位，请检查是否复制完整")
        try:
            port = int(self.advanced_vars["port"].get().strip())
        except ValueError as exc:
            raise ValueError("端口必须是数字") from exc
        if not 1 <= port <= 65535:
            raise ValueError("端口必须在 1 到 65535 之间")
        values.update({
            "rtc_api_version": self.advanced_vars["rtc_api_version"].get().strip() or API_VERSION_DEFAULT,
            "bind_host": self.advanced_vars["bind_host"].get().strip() or "0.0.0.0",
            "port": port,
        })
        ark_api_key = self.ai_vars["ark_api_key"].get().strip()
        if ark_api_key:
            values["ark_api_key"] = ark_api_key
        values["ark_model"] = self.ai_vars["ark_model"].get().strip() or DEFAULT_ARK_MODEL
        return values

    def _editor_template(self) -> dict[str, Any]:
        try:
            value = json.loads(self.template_editor.get("1.0", "end"))
        except json.JSONDecodeError as exc:
            raise ValueError(f"智能体模板不是有效 JSON：{exc}") from exc
        if not isinstance(value, dict):
            raise ValueError("智能体模板顶层必须是对象")
        validate_template(value)
        return value

    def _save_first_setup(self) -> None:
        if self.proc and self.proc.poll() is None:
            messagebox.showwarning("请先停止服务", "运行中不能更换密钥或智能体配置。", parent=self.root)
            return
        try:
            config = self._collect_config()
            template = self._editor_template()
            write_json(CONFIG_PATH, config)
            write_json(TEMPLATE_PATH, template)
        except (OSError, ValueError) as exc:
            messagebox.showerror("保存失败", str(exc), parent=self.root)
            return
        self._load_template_editor()
        self._set_notice("设置完成，可以启动小琴服务", "ok")
        self._append_log("首次设置已安全保存。\n", "ok")
        messagebox.showinfo("设置完成", "配置已保存在本机。现在可以启动小琴服务。", parent=self.root)
        self.show_page("home")

    def _format_template(self) -> None:
        try:
            value = json.loads(self.template_editor.get("1.0", "end"))
        except json.JSONDecodeError as exc:
            messagebox.showerror("无法格式化", str(exc), parent=self.root)
            return
        self.template_editor.delete("1.0", "end")
        self.template_editor.insert("1.0", json.dumps(value, ensure_ascii=False, indent=2))

    def _save_advanced(self) -> None:
        self._save_first_setup()

    def start_service(self) -> None:
        if self.proc and self.proc.poll() is None:
            return
        if not private_ready():
            self.show_page("home")
            self._set_notice("配置尚未完成，可让 AI 自动检查或进入手动设置", "warn")
            self._ai_append(
                "系统",
                "基础配置尚未完成。点击右侧“自动检查当前配置”，我会告诉你下一步。",
                "system",
            )
            return
        if not SERVER_SCRIPT.exists():
            messagebox.showerror("启动失败", f"缺少服务文件：{SERVER_SCRIPT}", parent=self.root)
            return
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0
        try:
            self.proc = subprocess.Popen(
                [sys.executable, "-u", str(SERVER_SCRIPT)], cwd=str(BASE_DIR),
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace", bufsize=1,
                creationflags=creationflags,
            )
        except OSError as exc:
            messagebox.showerror("启动失败", str(exc), parent=self.root)
            return
        self.start_button.set_enabled(False)
        self.stop_button.set_enabled(True)
        self._set_service_state("正在启动", "正在加载小琴智能体配置…", "warn")
        self._append_log(f"服务进程已启动（PID {self.proc.pid}）。\n", "gui")
        threading.Thread(target=self._read_process, daemon=True).start()
        threading.Thread(target=self._wait_process, daemon=True).start()
        self.root.after(450, self._check_health)

    def stop_service(self) -> None:
        proc = self.proc
        if not proc or proc.poll() is not None:
            self._after_process_exit()
            return
        self._append_log("正在停止服务…\n", "warn")
        try:
            proc.terminate()
        except OSError as exc:
            self._append_log(f"停止失败：{exc}\n", "error")
        threading.Thread(target=self._kill_later, args=(proc,), daemon=True).start()

    def _kill_later(self, proc: subprocess.Popen[str]) -> None:
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                proc.kill()
            except OSError:
                pass

    def _read_process(self) -> None:
        proc = self.proc
        if not proc or not proc.stdout:
            return
        for line in proc.stdout:
            self.log_queue.put(line)

    def _wait_process(self) -> None:
        proc = self.proc
        if not proc:
            return
        code = proc.wait()
        self.log_queue.put(f"服务已停止（返回码 {code}）。\n")
        if not self.closing:
            self.root.after(0, self._after_process_exit)

    def _after_process_exit(self) -> None:
        self.proc = None
        self.start_button.set_enabled(True)
        self.stop_button.set_enabled(False)
        self.service_reachable = False
        self._set_service_state("未启动", "服务目前处于关闭状态", "off")

    def _schedule_logs(self) -> None:
        self.root.after(120, self._drain_logs)

    def _drain_logs(self) -> None:
        try:
            while True:
                line = self.log_queue.get_nowait()
                lowered = line.lower()
                tag = "error" if "失败" in line or "error" in lowered else (
                    "warn" if "警告" in line or "warning" in lowered else "ok"
                )
                self._append_log(line, tag)
                if "post /startvoicechat" in lowered or "智能体启动请求已接受" in line:
                    self.device_seen = True
                    self.step_labels[1].configure(text="开发板已连接并请求智能体", fg=GREEN)
                    self.step_labels[2].configure(text="智能体在线，可以唤醒对话", fg=GREEN)
        except queue.Empty:
            pass
        if not self.closing:
            self.root.after(120, self._drain_logs)

    def _append_log(self, text: str, tag: str = "gui") -> None:
        self.log_view.insert("end", text, tag)
        try:
            count = int(self.log_view.index("end-1c").split(".")[0])
        except ValueError:
            count = 0
        if count > LOG_LIMIT:
            self.log_view.delete("1.0", f"{count - LOG_LIMIT}.0")
        self.log_view.see("end")

    def _schedule_health(self) -> None:
        self.root.after(1800, self._health_tick)

    def _health_tick(self) -> None:
        if self.closing:
            return
        self._check_health()
        self.root.after(2200, self._health_tick)

    def _check_health(self) -> None:
        if self.health_busy:
            return
        try:
            config = read_json(CONFIG_PATH)
            port = int(config.get("port", PORT_DEFAULT))
        except (OSError, ValueError, json.JSONDecodeError):
            return
        self.health_busy = True
        threading.Thread(target=self._health_worker, args=(port,), daemon=True).start()

    def _health_worker(self, port: int) -> None:
        status, payload, error = health_request(port)
        if not self.closing:
            self.root.after(0, lambda: self._health_done(status, payload, error))

    def _health_done(self, status: int, payload: dict[str, Any] | None,
                     _error: str | None) -> None:
        self.health_busy = False
        if status == 200 and isinstance(payload, dict):
            self.service_reachable = True
            data = payload.get("data", {}) if isinstance(payload.get("data", {}), dict) else {}
            self.model_var.set(str(data.get("model", "已连接")))
            speaker = str(data.get("speaker", ""))
            self.voice_var.set("俏皮女声" if "qiaopinvsheng" in speaker else (speaker or "已开启"))
            self._set_service_state("服务在线", "小琴已准备好，等待开发板连接", "on")
            if not self.proc or self.proc.poll() is not None:
                self.start_button.set_enabled(False)
                self.stop_button.set_enabled(False)
                self.service_detail.set("服务正在另一个窗口运行")
            self.step_labels[0].configure(text="本地服务运行正常", fg=GREEN)
            if not self.device_seen:
                self.step_labels[1].configure(text="等待开发板连接热点", fg=GOLD_DARK)
        elif self.proc and self.proc.poll() is None:
            self._set_service_state("正在启动", "服务进程已运行，等待接口就绪…", "warn")
        else:
            self.service_reachable = False
            self.start_button.set_enabled(True)
            self.stop_button.set_enabled(False)
            self._set_service_state("未启动", "服务目前处于关闭状态", "off")

    def _set_service_state(self, title: str, detail: str, state: str) -> None:
        self.service_title.set(title)
        self.service_detail.set(detail)
        if state == "on":
            text, bg, fg = "● 服务在线", GREEN_SOFT, GREEN
        elif state == "warn":
            text, bg, fg = "● 正在准备", GOLD_SOFT, GOLD_DARK
        else:
            text, bg, fg = "● 未启动", RED_SOFT, RED
        self.header_status.configure(text=text, bg=bg, fg=fg)

    def _set_notice(self, text: str, tone: str) -> None:
        colors = {"ok": (GREEN_SOFT, GREEN), "warn": (GOLD_SOFT, GOLD_DARK),
                  "error": (RED_SOFT, RED)}
        bg, fg = colors.get(tone, (PALE_BLUE, NAVY))
        self.notice.configure(text=text, bg=bg, fg=fg)

    def _copy_board_url(self) -> None:
        self.root.clipboard_clear()
        self.root.clipboard_append(BOARD_URL)
        self._set_notice("开发板地址已复制", "ok")

    def _copy_logs(self) -> None:
        self.root.clipboard_clear()
        self.root.clipboard_append(self.log_view.get("1.0", "end"))

    def _clear_logs(self) -> None:
        self.log_view.delete("1.0", "end")

    def _on_close(self) -> None:
        if self.proc and self.proc.poll() is None:
            if not messagebox.askyesno(
                "退出小琴桌面服务",
                "退出会同时停止开发板正在使用的本地服务。确定退出吗？",
                parent=self.root,
            ):
                return
        self.closing = True
        proc = self.proc
        if proc and proc.poll() is None:
            try:
                proc.terminate()
            except OSError:
                pass
        self.root.destroy()


def main(initial_page: str = "home", autostart: bool = False) -> int:
    if not SERVER_SCRIPT.exists():
        print(f"缺少 server.py：{SERVER_SCRIPT}", file=sys.stderr)
        return 2
    root = Tk()
    ConsumerApp(root, initial_page=initial_page, autostart=autostart)
    root.mainloop()
    return 0


if __name__ == "__main__":
    page = "setup" if "--setup" in sys.argv else "home"
    raise SystemExit(main(page, "--autostart" in sys.argv))
