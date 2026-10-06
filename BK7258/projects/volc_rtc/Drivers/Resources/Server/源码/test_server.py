"""Local protocol regression checks. No firmware build or cloud requests."""
import copy
import json
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import patch

import server


class LocalVoiceProtocolTests(unittest.TestCase):
    def setUp(self):
        self.settings = {
            "rtc_app_id": "a" * 24,
            "rtc_app_key": "test-only-key",
            "start_template": {
                "Config": {"LLMConfig": {"Mode": "CustomLLM"}},
                "AgentConfig": {"WelcomeMessage": "我是小琴，今天准备整什么活？"},
            },
        }
        self.room = server.create_room(self.settings, {"audio_codec": "OPUS"})
        self.request = {k: self.room[k] for k in ("app_id", "room_id", "task_id")}
        self.request["wake_sequence"] = 1

    def test_deferred_greeting_does_not_mutate_private_template(self):
        original = copy.deepcopy(self.settings)
        body = server.build_start_body(self.settings, self.room, {"defer_welcome": True})
        self.assertEqual(body["AgentConfig"]["WelcomeMessage"], "")
        self.assertEqual(body["AgentConfig"]["TargetUserId"], [self.room["uid"]])
        self.assertEqual(self.settings, original)

    def test_old_firmware_keeps_join_greeting(self):
        body = server.build_start_body(self.settings, self.room, {})
        self.assertEqual(body["AgentConfig"]["WelcomeMessage"],
                         self.settings["start_template"]["AgentConfig"]["WelcomeMessage"])

    def test_qwen_import_is_rejected_without_any_network_call(self):
        template = {"Config": {"LLMConfig": {"Mode": "CustomLLM", "ModelName": "qwen3.7-flash"}}}
        with self.assertRaises(server.ConfigurationError):
            server.validate_voice_template(template)

    def test_disabling_automatic_speech_is_rejected(self):
        for name in ("LLMConfig", "TTSConfig"):
            with self.subTest(name=name), self.assertRaises(server.ConfigurationError):
                server.validate_voice_template({"Config": {name: {"AutoActive": False}}})

    def test_health_profile_excludes_credentials(self):
        self.settings["start_template"]["Config"]["LLMConfig"].update(
            Mode="ArkV3", ModelName="doubao-seed-2-0-lite-260215", APIKey="test-secret")
        profile = server.voice_profile(self.settings)
        self.assertEqual(profile["model"], "doubao-seed-2-0-lite-260215")
        self.assertNotIn("test-secret", json.dumps(profile))

    @patch("server.call_rtc_api", return_value=(200, {"Result": "ok"}))
    def test_text_prompt_is_sent_to_llm_not_just_tts(self, api):
        self.start_local_server()
        self.url = self.url.replace("/wakeupvoicechat", "/updatevoicechat")
        body = dict(self.request, command="external_text_to_llm", message="你好小琴", interrupt_mode=1)
        self.assertEqual(self.post(body)[0], 200)
        self.assertEqual(api.call_args.args[2]["Command"], "ExternalTextToLLM")

    def post(self, request=None, auth=True):
        body = json.dumps(request or self.request).encode()
        headers = {"Content-Type": "application/json"}
        if auth:
            headers["Authorization"] = "af78e30" + self.settings["rtc_app_id"]
        req = urllib.request.Request(self.url, data=body, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=5) as response:
                return response.status, json.load(response)
        except urllib.error.HTTPError as exc:
            return exc.code, json.load(exc)

    def start_local_server(self):
        self.http = server.LocalAgentServer(("127.0.0.1", 0), self.settings)
        self.url = f"http://127.0.0.1:{self.http.server_port}/wakeupvoicechat"
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.http.server_close)
        self.addCleanup(self.thread.join, 2)
        self.addCleanup(self.http.shutdown)

    @patch("server.call_rtc_api", return_value=(200, {"Result": "ok"}))
    def test_wakeup_speaks_once_per_sequence(self, api):
        self.start_local_server()
        self.assertEqual(self.post()[0], 200)
        self.assertEqual(self.post()[0], 200)
        self.assertEqual(api.call_count, 1)
        args = api.call_args.args
        self.assertEqual(args[1], "UpdateVoiceChat")
        self.assertEqual(args[2]["Command"], "ExternalTextToSpeech")
        self.assertEqual(args[2]["InterruptMode"], 1)
        self.assertEqual(args[2]["Message"],
                         self.settings["start_template"]["AgentConfig"]["WelcomeMessage"])
        self.request["wake_sequence"] = 2
        self.assertEqual(self.post()[0], 200)
        self.assertEqual(api.call_count, 2)

    @patch("server.call_rtc_api", return_value=(200, {"ResponseMetadata": {
        "Error": {"Code": "TestFailure", "Message": "test failure"}}}))
    def test_cloud_error_is_not_reported_as_success(self, api):
        self.start_local_server()
        self.assertEqual(self.post()[0], 502)
        self.assertEqual(self.post()[0], 502)
        self.assertEqual(api.call_count, 2)

    @patch("server.call_rtc_api")
    def test_missing_auth_never_calls_cloud(self, api):
        self.start_local_server()
        self.assertEqual(self.post(auth=False)[0], 401)
        api.assert_not_called()

    @patch("server.call_rtc_api")
    def test_wrong_app_and_sequence_never_call_cloud(self, api):
        self.start_local_server()
        wrong = dict(self.request, app_id="wrong")
        self.assertEqual(self.post(wrong)[0], 400)
        wrong = dict(self.request, wake_sequence=0)
        self.assertEqual(self.post(wrong)[0], 400)
        api.assert_not_called()


if __name__ == "__main__":
    unittest.main()
