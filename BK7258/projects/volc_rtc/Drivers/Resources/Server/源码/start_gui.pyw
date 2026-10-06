"""双击启动“小琴桌面服务”（无黑色控制台窗口）。"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

try:
    from consumer_gui import main
except ImportError as exc:
    print("无法导入 consumer_gui.py：", exc, file=sys.stderr)
    print("请确认本目录文件完整，且 Python 是 3.9+ 标准安装。", file=sys.stderr)
    input("按回车关闭窗口...")
    sys.exit(1)

initial_page = "setup" if "--setup" in sys.argv else "home"
raise SystemExit(main(initial_page, "--autostart" in sys.argv))
