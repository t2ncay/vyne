import unittest
import json

from scripts.test_esp32_screen import ROOT, PATTERNS, firmware_result, screen_properties
from scripts.test_esp32 import classify, merge_build_properties


def profile():
    return {"build_properties": [
        "build.extra_flags=-DUSER_SETUP_LOADED -DILI9488_DRIVER -DTFT_MOSI=23 "
        "-DTFT_MISO=19 -DTFT_SCLK=18 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=4 -DTOUCH_CS=21"
    ]}


class ScreenPropertiesTests(unittest.TestCase):
    def test_hw950_full_panel_profile_preserves_core_flags_and_orientation(self):
        source = json.loads((ROOT / "vyne/runtime/esp32/profiles/hw950-screen-rgb.json").read_text())
        result = screen_properties(source)[0]
        self.assertTrue(result.startswith("compiler.cpp.extra_flags="))
        for flag in ("-DST7796_DRIVER", "-DTFT_RGB_ORDER=1", "-DVYNE_DISPLAY_MIRROR_Y=1",
                     "-DVYNE_DISPLAY_ENABLE_PIN=27", "-DTFT_WIDTH=240", "-DTFT_HEIGHT=320"):
            self.assertIn(flag, result)

    def test_profile_and_cli_compiler_flags_are_combined(self):
        result = merge_build_properties(["compiler.cpp.extra_flags=-DTFT_RGB_ORDER=1",
                                         "compiler.cpp.extra_flags=-DVYNE_TOUCH_REFERENCE_XPT2046"])
        self.assertEqual(result, ["compiler.cpp.extra_flags=-DTFT_RGB_ORDER=1 -DVYNE_TOUCH_REFERENCE_XPT2046"])

    def test_driver_override_removes_original_driver(self):
        result = screen_properties(profile(), driver="st7796")[0]
        self.assertIn("-UILI9341_DRIVER", result)
        self.assertIn("-DST7796_DRIVER", result)

    def test_cyd_reference_profile_preserves_rgb_and_backlight(self):
        source = json.loads((ROOT / "vyne/runtime/esp32/profiles/cyd-ili9341-rgb.json").read_text())
        result = screen_properties(source)[0]
        for flag in ("-DILI9341_DRIVER", "-DTFT_RGB_ORDER=1", "-DUSE_HSPI_PORT",
                     "-DTFT_BL=21", "-DTFT_BACKLIGHT_ON=1", "-DTFT_RST=-1", "-DVYNE_DISPLAY_ENABLE_PIN=27",
                     "-UTOUCH_CS", "-UVYNE_TOUCH_XPT2046"):
            self.assertIn(flag, result)

    def test_screen_only_does_not_guess_backlight_or_mutate_profile(self):
        source = profile()
        original = source["build_properties"][0]
        result = screen_properties(source)
        self.assertIn("-UTOUCH_CS", result[0])
        self.assertNotIn("-DTFT_BL=", result[0])
        self.assertEqual(source["build_properties"][0], original)

    def test_confirmed_backlight_replaces_definitions_and_sets_polarity(self):
        result = screen_properties(profile(), 27, "low")
        self.assertIn("-UTFT_BL -UTFT_BACKLIGHT_ON -DTFT_BL=27 -DTFT_BACKLIGHT_ON=0", result[0])

    def test_backlight_cannot_replace_spi_or_control_signal(self):
        for pin in (23, 19, 18, 15, 2, 4):
            with self.subTest(pin=pin), self.assertRaisesRegex(ValueError, "conflicts"):
                screen_properties(profile(), pin)

    def test_flash_uart_and_input_only_pins_are_rejected(self):
        for pin in (1, 3, 6, 11, 34, 39):
            with self.subTest(pin=pin), self.assertRaises(ValueError):
                screen_properties(profile(), pin)

    def test_explicit_profile_required(self):
        with self.assertRaisesRegex(ValueError, "USER_SETUP_LOADED"):
            screen_properties({"build_properties": ["build.extra_flags=-DILI9488_DRIVER"]})

    def test_frequency_override_is_bounded(self):
        self.assertIn("-USPI_FREQUENCY -DSPI_FREQUENCY=1000000",
                      screen_properties(profile(), frequency=1000000)[0])
        with self.assertRaises(ValueError):
            screen_properties(profile(), frequency=0)


class FirmwareEvidenceTests(unittest.TestCase):
    def test_false_positive_reference_touch_contact_is_not_a_pass(self):
        text = "raw touch: x=0 y=0 z=4095 contact=1\ntouch probe: DONE\n"
        self.assertEqual(classify("touch_probe", text)[0], "SKIP")

    def test_old_scan_completion_cannot_pass_screen_demo(self):
        self.assertEqual(firmware_result("Scan complete.\n", "auto")[0], "FAIL")

    def test_ready_marker_without_patterns_is_insufficient(self):
        self.assertEqual(firmware_result("SCREEN_DEMO_READY\n", "auto")[0], "FAIL")

    def test_panic_after_drawing_still_fails(self):
        text = "SCREEN_DEMO_READY\nSCREEN_STAGE:TEXT\nGuru Meditation Error\n"
        self.assertEqual(firmware_result(text, "text")[0], "FAIL")

    def test_complete_sequence_only_confirms_drawing_commands(self):
        text = "SCREEN_DEMO_READY\r\n" + "".join(
            f"SCREEN_STAGE:{name.upper()}\r\n" for name in PATTERNS
        )
        result, reason = firmware_result(text, "auto")
        self.assertEqual(result, "PASS")
        self.assertIn("physical rendering requires visual confirmation", reason)

    def test_selected_pattern_must_be_observed(self):
        text = "SCREEN_DEMO_READY\nSCREEN_STAGE:RED\n"
        self.assertEqual(firmware_result(text, "red")[0], "PASS")
        self.assertEqual(firmware_result(text, "white")[0], "FAIL")


if __name__ == "__main__":
    unittest.main()
