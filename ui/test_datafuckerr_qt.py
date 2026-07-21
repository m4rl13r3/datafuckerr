import os
import sys
import time
import unittest
from unittest import mock

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication, QFileDialog, QMessageBox

from ui.datafuckerr_qt import DatafuckerrWindow, build_stylesheet


class TestDatafuckerrWindow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.application = QApplication.instance() or QApplication([])

    def setUp(self):
        self.window = DatafuckerrWindow("/missing/path/diskpurge")
        self.window.show()
        self.application.processEvents()

    def tearDown(self):
        self.window.close()
        self.application.processEvents()

    def test_main_navigation(self):
        self.assertEqual(self.window.pages.count(), 2)
        for index, page in enumerate(("erase", "reports")):
            self.window.navigate(page)
            self.assertEqual(self.window.pages.currentIndex(), index)
        for page in ("erase", "reports"):
            self.window.navigate(page)
            self.assertTrue(self.window.navigation_buttons[page].isChecked())

    def test_critical_controls_are_accessible(self):
        controls = (
            self.window.device_selector,
            self.window.erase_target,
            self.window.entered_identifier,
            self.window.operator,
            self.window.witness,
            self.window.acknowledgment,
            self.window.erase_button,
        )
        for control in controls:
            self.assertTrue(control.accessibleName())

    def test_destructive_action_starts_locked(self):
        self.assertFalse(self.window.erase_button.isEnabled())
        self.assertIsNone(self.window.preparation_snapshot)

    def test_destructive_action_requires_every_confirmation(self):
        target = "/tmp/support-test.img"
        identifier = "identifiant-stable-abcdef"
        self.window.erase_target.blockSignals(True)
        self.window.erase_target.setText(target)
        self.window.erase_target.blockSignals(False)
        self.window.preparation_snapshot = {
            "binary": "/tmp/diskpurge",
            "target": target,
            "method": self.window.erase_method.currentText(),
            "verification": self.window.erase_verification.currentText(),
            "inspection": {"Identifiant": identifier},
        }
        self.window.erase_log.setText("/tmp/audit-test.jsonl")
        self.window.entered_identifier.setText("abcdef")
        self.window.operator.setText("OPÉRATEUR-TEST")
        self.window.witness.setText("TÉMOIN-TEST")
        self.assertFalse(self.window.erase_button.isEnabled())
        self.window.acknowledgment.setChecked(True)
        self.assertTrue(self.window.erase_button.isEnabled())
        self.window.witness.setText("OPÉRATEUR-TEST")
        self.assertFalse(self.window.erase_button.isEnabled())

    def test_final_dialog_is_destructive_and_cancellable(self):
        dialog = self.window._create_final_confirmation(
            "/tmp/support-test.img", "identifiant-test"
        )
        cancel_button = dialog.button(QMessageBox.StandardButton.Cancel)
        erase_button = dialog.button(QMessageBox.StandardButton.Yes)
        self.assertIs(dialog.defaultButton(), cancel_button)
        self.assertEqual(erase_button.property("role"), "danger")
        self.assertEqual(cancel_button.property("role"), "secondary")
        self.assertIn("/tmp/support-test.img", dialog.informativeText())
        dialog.close()

    def test_initial_flow_hides_advanced_controls(self):
        self.window.navigate("erase")
        self.assertEqual(set(self.window.navigation_buttons), {"erase", "reports"})
        self.assertTrue(self.window.options_area.isHidden())
        self.assertTrue(self.window.section_confirmation.isHidden())
        self.assertTrue(self.window.technical_area.isHidden())
        self.assertFalse(self.window.prepare_button.isEnabled())
        self.window.device_selector.addItem("Support de test", "/tmp/support-test.img")
        self.window.device_selector.setCurrentIndex(1)
        self.assertEqual(self.window.erase_target.text(), "/tmp/support-test.img")
        self.assertTrue(self.window.prepare_button.isEnabled())

    def test_scale_and_contrast(self):
        initial_style = self.window.styleSheet()
        self.window.adjust_scale(0.1)
        self.assertEqual(self.window.scale, 1.1)
        self.assertNotEqual(self.window.styleSheet(), initial_style)
        self.window.toggle_contrast()
        self.assertTrue(self.window.high_contrast)
        self.assertIn("#1B1F23", self.window.styleSheet())
        self.window.reset_scale()
        self.assertEqual(self.window.scale, 1.0)

    def test_shadcn_theme_stays_neutral(self):
        style = build_stylesheet()
        for forbidden_decoration in (
            "qlineargradient",
            "border-top",
            "border-left",
            "#1769D2",
            "#2188E5",
        ):
            self.assertNotIn(forbidden_decoration, style)
        self.assertIn("#171717", style)
        self.assertIn("#E5E5E5", style)
        self.assertIsNone(self.window.selection_section.graphicsEffect())

    def test_launch_failure_releases_interface(self):
        result = []
        self.window.run_command(
            ["/missing/path/datafuckerr-command"],
            self.window.erase_console,
            lambda code, output, error: result.append((code, output, error)),
        )
        deadline = time.monotonic() + 2
        while not result and time.monotonic() < deadline:
            self.application.processEvents()
            time.sleep(0.01)
        self.assertTrue(result)
        self.assertEqual(result[0][0], -1)
        self.assertIsNotNone(result[0][2])
        self.assertIsNone(self.window.process)
        self.assertTrue(self.window.detect_button.isEnabled())
        self.assertFalse(self.window.prepare_button.isEnabled())

    def test_frozen_report_uses_embedded_entry_point(self):
        self.window.run_command = mock.Mock()
        with (
            mock.patch.object(
                QFileDialog,
                "getSaveFileName",
                return_value=("/tmp/rapport.pdf", ""),
            ),
            mock.patch.object(sys, "frozen", True, create=True),
        ):
            self.window._after_report_precheck(
                "/tmp/diskpurge", "/tmp/audit.jsonl", 0, "", None
            )
        command = self.window.run_command.call_args.args[0]
        self.assertEqual(command[:2], [sys.executable, "--generate-report"])
        self.assertIn("/tmp/audit.jsonl", command)
        self.assertIn("/tmp/diskpurge", command)


if __name__ == "__main__":
    unittest.main()
