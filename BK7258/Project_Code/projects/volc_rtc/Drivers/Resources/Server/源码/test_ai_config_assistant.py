import json
import unittest
from unittest.mock import patch

from ai_config_assistant import build_snapshot, request_plan


class _Response:
    status = 200

    def __init__(self, payload):
        self.payload = payload

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def read(self):
        return json.dumps(self.payload).encode("utf-8")


class AiConfigAssistantTests(unittest.TestCase):
    def test_snapshot_redacts_all_nested_secrets_without_authorization(self):
        snapshot = build_snapshot(
            {"rtc_app_key": "rtc-secret", "port": 8080},
            {
                "Config": {
                    "APIKey": "model-secret",
                    "ProviderParams": '{"api_key":"nested-secret"}',
                    "ModelName": "doubao",
                }
            },
            "accidentally logged rtc-secret and nested-secret",
            False,
        )
        serialized = json.dumps(snapshot, ensure_ascii=False)
        self.assertNotIn("rtc-secret", serialized)
        self.assertNotIn("model-secret", serialized)
        self.assertNotIn("nested-secret", serialized)
        self.assertIn("8080", serialized)

    def test_snapshot_includes_plaintext_only_after_authorization(self):
        snapshot = build_snapshot(
            {"rtc_app_key": "rtc-secret"},
            {"Config": {"APIKey": "model-secret"}},
            "",
            True,
        )
        self.assertEqual(snapshot["local_config"]["rtc_app_key"], "rtc-secret")
        self.assertEqual(snapshot["voice_agent_template"]["Config"]["APIKey"], "model-secret")

    def test_request_plan_parses_tool_calls(self):
        payload = {
            "choices": [{
                "message": {
                    "content": "我先检查一下。",
                    "tool_calls": [{
                        "id": "call-1",
                        "type": "function",
                        "function": {
                            "name": "validate_configuration",
                            "arguments": "{}",
                        },
                    }],
                }
            }]
        }
        with patch("urllib.request.urlopen", return_value=_Response(payload)) as mocked:
            content, actions = request_plan(
                "ark-test-key", "doubao-test", "帮我检查", {"secret_access": "未授权"}
            )
        self.assertEqual(content, "我先检查一下。")
        self.assertEqual(actions[0]["name"], "validate_configuration")
        request = mocked.call_args.args[0]
        self.assertEqual(request.get_header("Authorization"), "Bearer ark-test-key")


if __name__ == "__main__":
    unittest.main()
