import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import api.howru as howru


class ProxyAlertTests(unittest.TestCase):
    def setUp(self):
        howru.proxy_alert_state = {}
        howru.use_proxy_alerting = True
        self.temp_dir = tempfile.mkdtemp(prefix="almond-alert-tests-")
        howru.proxy_alert_state_file = os.path.join(self.temp_dir, "proxy_alert_state.json")

    def test_proxy_alert_transition_sends_once_per_state_change(self):
        payload = {
            "server": [{
                "host": {"name": "node-1"},
                "monitoring": [{
                    "name": "check_cpu",
                    "pluginName": "check_cpu",
                    "pluginStatusCode": "2",
                    "pluginStatus": "CRITICAL",
                    "pluginOutput": "CPU is critical",
                }],
            }]
        }

        with patch.object(howru, "load_proxy_alert_config", return_value={
            "send_alerts_to_slack": True,
            "slack_webhook_url": "https://hooks.slack.test/abc",
            "send_alerts_to_email": True,
            "smtp_url": "smtp://mail.test:25",
            "email_from": "almond@example.com",
            "email_recipient": "ops@example.com",
            "email_subject": "Almond alert",
        }), patch.object(howru, "send_slack_alert") as send_slack, patch.object(howru, "send_email_alert") as send_email:
            howru.proxy_alert_if_needed(payload)
            self.assertEqual(send_slack.call_count, 1)
            self.assertEqual(send_email.call_count, 1)

            howru.proxy_alert_if_needed(payload)
            self.assertEqual(send_slack.call_count, 1)
            self.assertEqual(send_email.call_count, 1)

    def test_proxy_alert_will_not_run_when_disabled_in_config(self):
        howru.use_proxy_alerting = False
        payload = {"server": [{"host": {"name": "node-2"}, "monitoring": [{"name": "check_disk", "pluginStatusCode": "2", "pluginOutput": "disk full"}]}]}

        with patch.object(howru, "load_proxy_alert_config", return_value={"send_alerts_to_slack": True, "slack_webhook_url": "https://x"}) as load_config, patch.object(howru, "send_slack_alert") as send_slack:
            howru.proxy_alert_if_needed(payload)
            load_config.assert_not_called()
            send_slack.assert_not_called()

    def test_proxy_alert_route_uses_server_and_check_identity(self):
        payload = {
            "server": [{
                "host": {"name": "web-01"},
                "monitoring": [{
                    "name": "check_cpu",
                    "pluginName": "check_cpu",
                    "pluginStatusCode": "2",
                    "pluginStatus": "CRITICAL",
                    "pluginOutput": "CPU is critical",
                }],
            }]
        }

        with patch.object(howru, "load_proxy_alert_config", return_value={
            "send_alerts_to_slack": True,
            "slack_webhook_url": "https://hooks.global.example/abc",
            "server.web-01.check_cpu.slack_webhook_url": "https://hooks.web01.example/abc",
        }), patch.object(howru, "send_slack_alert") as send_slack:
            howru.proxy_alert_if_needed(payload)
            self.assertEqual(send_slack.call_count, 1)
            self.assertEqual(send_slack.call_args[0][0], "https://hooks.web01.example/abc")

    def test_format_alert_email_html_generates_valid_html(self):
        html = howru.format_alert_email_html(
            server_name="web-01",
            check_name="check_cpu",
            status_code=2,
            output="CPU usage is 95%",
            message="[Almond] web-01: check_cpu is CRITICAL. CPU usage is 95%",
            status_label="CRITICAL",
        )
        self.assertIn("<html>", html)
        self.assertIn("</html>", html)
        self.assertIn("web-01", html)
        self.assertIn("check_cpu", html)
        self.assertIn("CRITICAL", html)
        self.assertIn("CPU usage is 95%", html)
        self.assertIn("#dc3545", html)  # Critical red color


if __name__ == "__main__":
    unittest.main()
