"""消费者界面的无网络回归测试。"""

import json
import unittest

try:
    import consumer_gui
except ModuleNotFoundError as exc:
    if exc.name == "tkinter":
        consumer_gui = None
    else:
        raise


@unittest.skipIf(consumer_gui is None, "当前构建环境未安装 Tkinter；界面测试在 Windows 执行")
class ConsumerGuiTests(unittest.TestCase):
    def test_extracts_json_from_console_snippet(self):
        source = 'POST https://example.invalid\n' + json.dumps({
            "Config": {"LLMConfig": {"ModelName": "doubao-test"}},
            "AgentConfig": {"WelcomeMessage": "你好"},
        }, ensure_ascii=False)
        self.assertEqual(
            consumer_gui.extract_json(source)["AgentConfig"]["WelcomeMessage"],
            "你好",
        )

    def test_rejects_non_agent_clipboard_content(self):
        with self.assertRaises(ValueError):
            consumer_gui.extract_json('{"hello": "world"}')

    def test_rejects_qwen_and_disabled_tts(self):
        with self.assertRaises(ValueError):
            consumer_gui.validate_template({
                "Config": {"LLMConfig": {"ModelName": "qwen3.7-flash"}}
            })
        with self.assertRaises(ValueError):
            consumer_gui.validate_template({
                "Config": {"TTSConfig": {"AutoActive": False}}
            })

    def test_reads_consumer_facing_profile(self):
        raw = json.dumps({"req_params": {"speaker": "xiaoqin"}})
        model, tts, speaker = consumer_gui.template_summary({
            "Config": {
                "LLMConfig": {"ModelName": "doubao-test"},
                "TTSConfig": {
                    "AutoActive": True,
                    "ProviderParams": {"VolcanoTTSParameters": raw},
                },
            }
        })
        self.assertEqual((model, tts, speaker), ("doubao-test", "已开启", "xiaoqin"))


if __name__ == "__main__":
    unittest.main()
