#!/usr/bin/env python3
"""Project_Chen 本地智能体服务 - GUI 控制器（纯标准库 / Tkinter）。

只使用 Python 自带的 tkinter，不需要 pip install 任何第三方包，也不需要
打包成 exe —— 直接双击 start_gui.pyw 即可启动。

保留 server.py 不变，本程序以子进程方式启动 server.py 并把 stdout/stderr
重定向到 GUI 日志面板，同时提供启动/停止、健康检查、配置编辑等图形化能力。

Tkinter 在 Windows 上没有原生系统托盘 API，因此关闭窗口会先弹"确认退出"
对话框；不想用托盘是 PySide6 方案的明确退化点。
"""

from __future__ import annotations

import json
import queue
import subprocess
import sys
import threading
import urllib.error
import urllib.request
from pathlib import Path
from tkinter import (
    BooleanVar,
    Button,
    Canvas,
    Entry,
    Frame,
    Label,
    LabelFrame,
    StringVar,
    Tk,
    messagebox,
    ttk,
)
from tkinter.scrolledtext import ScrolledText
from typing import Any

# ---------- 路径常量 ----------

BASE_DIR = (
    Path(sys.executable).resolve().parent
    if getattr(sys, "frozen", False)
    else Path(__file__).resolve().parent
)
PRIVATE_DIR = BASE_DIR / "private"
CONFIG_PATH = PRIVATE_DIR / "server_config.json"
TEMPLATE_PATH = PRIVATE_DIR / "start_voice_chat_template.json"
SERVER_SCRIPT = BASE_DIR / "server.py"
WINDOW_TITLE = "Project_Chen 本地智能体服务"

# (字段 key, 显示标签, 是否敏感, 占位提示)
CONFIG_FIELDS: list[tuple[str, str, bool, str]] = [
    ("volc_access_key_id",     "AccessKey ID",       True,  "请填写火山引擎 AccessKey ID"),
    ("volc_secret_access_key", "SecretAccessKey",    True,  "请填写火山引擎 SecretAccessKey"),
    ("rtc_app_id",             "RTC App ID",         False, "24 位字符串"),
    ("rtc_app_key",            "RTC App Key",        True,  "请填写 Project_Chen 的 RTC AppKey"),
    ("rtc_api_version",        "RTC API 版本",       False, "例如 2025-06-01"),
    ("bind_host",              "绑定地址",           False, "0.0.0.0 或 127.0.0.1"),
    ("port",                   "端口",               False, ""),
]

HEALTH_INTERVAL_MS = 2000
LOG_DRAIN_INTERVAL_MS = 150
LOG_MAX_LINES = 5000


# ---------- 工具 ----------

def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    text = json.dumps(data, ensure_ascii=False, indent=2)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text + "\n", encoding="utf-8")


def is_private_ready() -> bool:
    return CONFIG_PATH.exists() and TEMPLATE_PATH.exists()


def http_health(port: int, timeout: float = 1.5) -> tuple[int, dict[str, Any] | None, str | None]:
    url = f"http://127.0.0.1:{port}/health"
    req = urllib.request.Request(url)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
            return resp.status, json.loads(raw) if raw else {}, None
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8", errors="replace")
        try:
            payload: dict[str, Any] = json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            payload = {"message": raw or str(exc)}
        return exc.code, payload, None
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        return 0, None, str(exc)


# ---------- 主窗口 ----------

class App:

    def __init__(self, root: Tk) -> None:
        self.root = root
        self.root.title(WINDOW_TITLE)
        self.root.geometry("1000x720")
        self.root.minsize(820, 560)

        self._proc: subprocess.Popen[str] | None = None
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._closing = False

        self._build_ui()
        self._schedule_log_drain()
        self._schedule_health()

        self._set_status("未运行", running=False)
        self._refresh_health_view({"状态": "尚未启动服务"})

        if not is_private_ready():
            self._append_log(
                "[GUI] 检测到 private/ 缺失或不全，请先在「配置」页填写。\n"
            )

        # 拦截关闭按钮
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ----- UI 构建 -----

    def _build_ui(self) -> None:
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill="both", expand=True, padx=8, pady=8)
        self._notebook = notebook
        notebook.add(self._build_dashboard(notebook), text="仪表板")
        notebook.add(self._build_logs(notebook), text="日志")
        notebook.add(self._build_config(notebook), text="配置")

    def _build_dashboard(self, parent) -> Frame:
        page = Frame(parent)
        page.columnconfigure(0, weight=1)

        box = LabelFrame(page, text="服务状态", padx=10, pady=8)
        box.grid(row=0, column=0, sticky="ew", pady=(0, 8))

        self._status_canvas = Canvas(box, width=14, height=14, highlightthickness=0)
        self._status_canvas.create_oval(2, 2, 12, 12, fill="#c0392b", outline="")
        self._status_canvas.pack(side="left")
        self._status_var = StringVar(value="未运行")
        Label(
            box, textvariable=self._status_var,
            font=("Microsoft YaHei", 12, "bold"),
        ).pack(side="left", padx=8)
        self._stop_btn = Button(box, text="停止服务", command=self._on_stop,
                                width=12, state="disabled")
        self._stop_btn.pack(side="right", padx=4)
        self._start_btn = Button(box, text="启动服务", command=self._on_start, width=12)
        self._start_btn.pack(side="right", padx=4)

        health_box = LabelFrame(page, text="健康检查 (/health)", padx=10, pady=8)
        health_box.grid(row=1, column=0, sticky="nsew", pady=(0, 8))
        page.rowconfigure(1, weight=1)
        toolbar = Frame(health_box)
        toolbar.pack(fill="x")
        Button(toolbar, text="立即检查", command=self._check_health).pack(side="left")
        self._autorefresh_var = BooleanVar(value=False)
        ttk.Checkbutton(
            toolbar, text="每 2 秒自动刷新",
            variable=self._autorefresh_var,
        ).pack(side="left", padx=8)
        self._health_view = ScrolledText(health_box, height=8, font=("Consolas", 10), wrap="none")
        self._health_view.pack(fill="both", expand=True, pady=(6, 0))

        profile_box = LabelFrame(page, text="当前模型配置（脱敏）", padx=10, pady=8)
        profile_box.grid(row=2, column=0, sticky="ew")
        self._profile_vars: dict[str, StringVar] = {}
        for i, key in enumerate((
            "app_id", "wake_protocol", "llm_mode", "model",
            "llm_active", "tts_active", "speaker", "asr_stream_mode",
        )):
            Label(profile_box, text=key + ":").grid(row=i, column=0, sticky="w", padx=(0, 8), pady=2)
            var = StringVar(value="—")
            self._profile_vars[key] = var
            Label(profile_box, textvariable=var, anchor="w").grid(row=i, column=1, sticky="w", pady=2)

        return page

    def _build_logs(self, parent) -> Frame:
        page = Frame(parent)
        toolbar = Frame(page)
        toolbar.pack(fill="x")
        self._autoscroll_var = BooleanVar(value=True)
        ttk.Checkbutton(
            toolbar, text="自动滚动到底部",
            variable=self._autoscroll_var,
        ).pack(side="left")
        Button(toolbar, text="复制全部", command=self._copy_all_logs).pack(side="right")
        Button(
            toolbar, text="清空",
            command=lambda: self._log_view.delete("1.0", "end"),
        ).pack(side="right", padx=4)

        self._log_view = ScrolledText(page, font=("Consolas", 10), wrap="none")
        self._log_view.pack(fill="both", expand=True, pady=(6, 0))
        return page

    def _build_config(self, parent) -> Frame:
        page = Frame(parent)
        page.columnconfigure(0, weight=1)
        page.rowconfigure(1, weight=1)

        server_box = LabelFrame(page, text="server_config.json", padx=10, pady=8)
        server_box.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        Label(
            server_box,
            text="修改前请先停止服务；保存后必须重新启动服务才能生效。\n"
                 "AccessKey / SecretAccessKey / RTC AppKey 为敏感字段，"
                 "输入时屏幕不显示字符。",
            fg="#555", justify="left", anchor="w",
        ).pack(fill="x")

        form = Frame(server_box)
        form.pack(fill="x", pady=(8, 4))
        self._config_vars: dict[str, StringVar] = {}
        self._secret_entries: list[Entry] = []
        for i, (key, label, secret, _placeholder) in enumerate(CONFIG_FIELDS):
            Label(form, text=label + ":").grid(row=i, column=0, sticky="w", padx=(0, 8), pady=2)
            var = StringVar()
            self._config_vars[key] = var
            entry = Entry(form, textvariable=var, width=60)
            if secret:
                entry.config(show="*")
                self._secret_entries.append(entry)
            entry.grid(row=i, column=1, sticky="we", pady=2)
            form.columnconfigure(1, weight=1)

        self._show_secret_var = BooleanVar(value=False)
        ttk.Checkbutton(
            server_box, text="显示密钥字段",
            variable=self._show_secret_var,
            command=self._on_toggle_show_secrets,
        ).pack(anchor="w")

        btn_row = Frame(server_box)
        btn_row.pack(fill="x", pady=(6, 0))
        Button(btn_row, text="从磁盘重新加载", command=self._reload_server_config).pack(side="left")
        Button(btn_row, text="保存", command=self._save_server_config).pack(side="right")

        tmpl_box = LabelFrame(page, text="start_voice_chat_template.json", padx=10, pady=8)
        tmpl_box.grid(row=1, column=0, sticky="nsew")
        tmpl_box.columnconfigure(0, weight=1)
        tmpl_box.rowconfigure(1, weight=1)
        Label(
            tmpl_box,
            text="完整 JSON 内容。保存时会校验合法性；修改后必须重启服务。",
            fg="#555", anchor="w",
        ).grid(row=0, column=0, sticky="ew")
        self._template_view = ScrolledText(tmpl_box, font=("Consolas", 10), wrap="none")
        self._template_view.grid(row=1, column=0, sticky="nsew", pady=(6, 0))
        tmpl_btn = Frame(tmpl_box)
        tmpl_btn.grid(row=2, column=0, sticky="ew", pady=(6, 0))
        Button(tmpl_btn, text="从磁盘重新加载", command=self._reload_template).pack(side="left")
        Button(tmpl_btn, text="格式化", command=self._format_template).pack(side="left", padx=4)
        Button(tmpl_btn, text="保存", command=self._save_template).pack(side="right")

        self._reload_server_config()
        self._reload_template()
        return page

    # ----- 服务进程管理 -----

    def _on_start(self) -> None:
        if self._proc and self._proc.poll() is None:
            return
        if not SERVER_SCRIPT.exists():
            messagebox.showerror(
                "找不到 server.py",
                f"server.py 不在以下目录：\n{SERVER_SCRIPT}",
            )
            return
        if not is_private_ready():
            if messagebox.askyesno(
                "配置缺失",
                "private/ 缺少 server_config.json 或 "
                "start_voice_chat_template.json，无法启动服务。\n\n"
                "是否切换到「配置」页？",
            ):
                self._notebook.select(2)
            return
        try:
            self._proc = subprocess.Popen(
                [sys.executable, "-u", str(SERVER_SCRIPT)],
                cwd=str(BASE_DIR),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=1,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except OSError as exc:
            messagebox.showerror("启动失败", str(exc))
            return
        self._append_log(f"[GUI] 已启动子进程 PID={self._proc.pid}\n")
        self._start_btn.config(state="disabled")
        self._stop_btn.config(state="normal")
        self._set_status("启动中…", running=True)
        threading.Thread(target=self._read_stdout, daemon=True).start()
        threading.Thread(target=self._wait_proc, daemon=True).start()
        # 500ms 后主动检查一次，状态能尽快从"启动中"变成"运行中"
        self.root.after(500, self._check_health)

    def _on_stop(self) -> None:
        proc = self._proc
        if not proc or proc.poll() is not None:
            self._after_proc_exit()
            return
        self._append_log("[GUI] 正在停止服务…\n")
        try:
            proc.terminate()
        except OSError as exc:
            self._append_log(f"[GUI] terminate 失败：{exc}\n")
        threading.Thread(target=self._force_kill_after, args=(proc, 3.0), daemon=True).start()

    def _force_kill_after(self, proc: subprocess.Popen[str], timeout: float) -> None:
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self._append_log("[GUI] 服务未在 3 秒内退出，强制结束。\n")
            try:
                proc.kill()
            except OSError:
                pass

    def _read_stdout(self) -> None:
        proc = self._proc
        if not proc or not proc.stdout:
            return
        for line in proc.stdout:
            self._log_queue.put(line)

    def _wait_proc(self) -> None:
        proc = self._proc
        if not proc:
            return
        rc = proc.wait()
        self._log_queue.put(f"[GUI] 子进程已退出，返回码={rc}\n")
        # Tkinter 只能在主线程改 widget；用 after 把回调丢回主线程
        self.root.after(0, self._after_proc_exit)

    def _after_proc_exit(self) -> None:
        self._proc = None
        self._start_btn.config(state="normal")
        self._stop_btn.config(state="disabled")
        self._set_status("未运行", running=False)
        self._refresh_health_view({"状态": "服务已停止"})

    # ----- 日志面板 -----

    def _schedule_log_drain(self) -> None:
        self.root.after(LOG_DRAIN_INTERVAL_MS, self._drain_log_queue)

    def _drain_log_queue(self) -> None:
        try:
            while True:
                line = self._log_queue.get_nowait()
                self._append_log(line)
        except queue.Empty:
            pass
        if not self._closing:
            self.root.after(LOG_DRAIN_INTERVAL_MS, self._drain_log_queue)

    def _append_log(self, text: str) -> None:
        self._log_view.insert("end", text)
        # 限制行数
        try:
            line_no = int(self._log_view.index("end-1c").split(".")[0])
        except ValueError:
            line_no = 0
        if line_no > LOG_MAX_LINES:
            self._log_view.delete("1.0", f"{line_no - LOG_MAX_LINES}.0")
        if self._autoscroll_var.get():
            self._log_view.see("end")

    def _copy_all_logs(self) -> None:
        self.root.clipboard_clear()
        self.root.clipboard_append(self._log_view.get("1.0", "end"))
        self._append_log("[GUI] 日志已复制到剪贴板。\n")

    # ----- 健康检查 -----

    def _schedule_health(self) -> None:
        self.root.after(HEALTH_INTERVAL_MS, self._health_tick)

    def _health_tick(self) -> None:
        if self._closing:
            return
        if self._autorefresh_var.get():
            self._check_health()
        self.root.after(HEALTH_INTERVAL_MS, self._health_tick)

    def _check_health(self) -> None:
        if not self._proc or self._proc.poll() is not None:
            self._refresh_health_view({"状态": "服务未运行，无法检查。"})
            return
        try:
            cfg = read_json(CONFIG_PATH)
            port = int(cfg.get("port", 8080))
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            self._refresh_health_view({"错误": f"读取配置失败：{exc}"})
            return
        threading.Thread(target=self._do_http_health, args=(port,), daemon=True).start()

    def _do_http_health(self, port: int) -> None:
        status, payload, err = http_health(port)
        self.root.after(0, lambda: self._on_health_done(status, payload, err))

    def _on_health_done(self, status: int, payload: dict[str, Any] | None, err: str | None) -> None:
        if err or payload is None:
            self._refresh_health_view({"状态": "请求失败", "错误": err or "空响应"})
            return
        # server.py 的 /health 返回 {code, msg, data: {...}}；
        # 健康 JSON 框显示完整 response，脱敏面板提取 data 字段。
        data = payload.get("data", payload) if isinstance(payload, dict) else {}
        self._refresh_health_view(payload, data)
        if data.get("app_id"):
            self._set_status("运行中", running=True)

    def _refresh_health_view(
        self, payload: dict[str, Any], data: dict[str, Any] | None = None
    ) -> None:
        self._health_view.delete("1.0", "end")
        self._health_view.insert("end", json.dumps(payload, ensure_ascii=False, indent=2))
        source = data if data is not None else payload
        for key, var in self._profile_vars.items():
            var.set(str(source.get(key, "—")))

    # ----- 配置：server_config.json -----

    def _reload_server_config(self) -> None:
        try:
            data = read_json(CONFIG_PATH)
        except FileNotFoundError:
            for var in self._config_vars.values():
                var.set("")
            self._append_log("[GUI] server_config.json 不存在。\n")
            return
        except (OSError, json.JSONDecodeError) as exc:
            messagebox.showwarning("加载失败", f"无法读取 server_config.json：\n{exc}")
            return
        for key, var in self._config_vars.items():
            var.set(str(data.get(key, "")))
        self._append_log("[GUI] 已重新加载 server_config.json。\n")

    def _save_server_config(self) -> None:
        try:
            data: dict[str, Any] = read_json(CONFIG_PATH) if CONFIG_PATH.exists() else {}
        except (OSError, json.JSONDecodeError) as exc:
            messagebox.showerror("读取失败", f"无法读取现有配置：\n{exc}")
            return
        for key, var in self._config_vars.items():
            data[key] = var.get().strip()
        try:
            write_json(CONFIG_PATH, data)
        except OSError as exc:
            messagebox.showerror("保存失败", f"无法写入 server_config.json：\n{exc}")
            return
        messagebox.showinfo(
            "已保存",
            "server_config.json 已保存。\n"
            "请先停止服务，再点击「启动服务」以使新配置生效。",
        )
        self._append_log("[GUI] 已保存 server_config.json。\n")

    def _on_toggle_show_secrets(self) -> None:
        show = "" if self._show_secret_var.get() else "*"
        for entry in self._secret_entries:
            entry.config(show=show)

    # ----- 配置：模板 -----

    def _reload_template(self) -> None:
        try:
            text = TEMPLATE_PATH.read_text(encoding="utf-8-sig")
        except FileNotFoundError:
            self._template_view.delete("1.0", "end")
            self._template_view.insert("end", "（private/start_voice_chat_template.json 不存在）")
            self._append_log("[GUI] start_voice_chat_template.json 不存在。\n")
            return
        except OSError as exc:
            messagebox.showwarning("加载失败", f"无法读取模板：\n{exc}")
            return
        self._template_view.delete("1.0", "end")
        self._template_view.insert("end", text)
        self._append_log("[GUI] 已重新加载 start_voice_chat_template.json。\n")

    def _format_template(self) -> None:
        try:
            data = json.loads(self._template_view.get("1.0", "end"))
        except json.JSONDecodeError as exc:
            messagebox.showwarning("格式化失败", f"JSON 不合法：\n{exc}")
            return
        self._template_view.delete("1.0", "end")
        self._template_view.insert("end", json.dumps(data, ensure_ascii=False, indent=2))

    def _save_template(self) -> None:
        text = self._template_view.get("1.0", "end")
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            messagebox.showwarning("保存失败", f"无法解析 JSON：\n{exc}")
            return
        if not isinstance(data, dict):
            messagebox.showwarning("保存失败", "JSON 顶层必须是对象。")
            return
        if "说明" in (data.get("Config") or {}):
            messagebox.showwarning(
                "占位符未替换",
                "Config 中仍含「说明」字段。"
                "请先用「小琴」智能体的代码示例替换模板内容。",
            )
            return
        try:
            write_json(TEMPLATE_PATH, data)
        except OSError as exc:
            messagebox.showerror("保存失败", f"无法写入模板：\n{exc}")
            return
        messagebox.showinfo(
            "已保存",
            "start_voice_chat_template.json 已保存。\n"
            "请先停止服务，再点击「启动服务」以使新模板生效。",
        )
        self._append_log("[GUI] 已保存 start_voice_chat_template.json。\n")

    # ----- 状态 -----

    def _set_status(self, text: str, *, running: bool) -> None:
        self._status_var.set(text)
        color = "#27ae60" if running else "#c0392b"
        self._status_canvas.itemconfigure(1, fill=color)

    # ----- 关闭 -----

    def _on_close(self) -> None:
        if self._proc and self._proc.poll() is None:
            if not messagebox.askyesno(
                "确认退出",
                "服务正在运行，退出后会停掉本机的 HTTP 接口。\n是否继续？",
            ):
                return
        self._closing = True
        try:
            self._on_stop()
        finally:
            self.root.destroy()


def main() -> int:
    if not SERVER_SCRIPT.exists():
        print(f"找不到 server.py：{SERVER_SCRIPT}", file=sys.stderr)
        return 2
    root = Tk()
    try:
        # 让 ttk 使用 Windows 原生主题，外观更专业
        style = ttk.Style(root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        elif "clam" in style.theme_names():
            style.theme_use("clam")
    except Exception:
        pass
    App(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())