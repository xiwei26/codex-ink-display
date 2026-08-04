import unittest

from local_server import make_dashboard_payload


def dashboard_rate_limits(rate_limits):
    payload = make_dashboard_payload({}, {}, {"rateLimits": rate_limits})
    return payload["rateLimits"]


class RateLimitWindowTests(unittest.TestCase):
    def test_identifies_weekly_window_when_it_is_primary(self):
        result = dashboard_rate_limits(
            {
                "primary": {"usedPercent": 55, "windowDurationMins": 10080, "resetsAt": 123},
                "secondary": None,
            }
        )

        self.assertIsNone(result["fiveHourUsedPercent"])
        self.assertEqual(result["weeklyUsedPercent"], 55.0)
        self.assertIsNone(result["fiveHourResetsAt"])
        self.assertEqual(result["weeklyResetsAt"], 123)

    def test_identifies_windows_regardless_of_field_order(self):
        result = dashboard_rate_limits(
            {
                "primary": {"usedPercent": 20, "windowDurationMins": 10080},
                "secondary": {"usedPercent": 75, "windowDurationMins": 300},
            }
        )

        self.assertEqual(result["fiveHourUsedPercent"], 75.0)
        self.assertEqual(result["weeklyUsedPercent"], 20.0)

    def test_does_not_guess_when_duration_is_missing(self):
        result = dashboard_rate_limits(
            {
                "primary": {"usedPercent": 40},
                "secondary": {"usedPercent": 60, "windowDurationMins": 1440},
            }
        )

        self.assertIsNone(result["fiveHourUsedPercent"])
        self.assertIsNone(result["weeklyUsedPercent"])


if __name__ == "__main__":
    unittest.main()
