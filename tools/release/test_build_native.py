import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.release import build_native


class AppImageBuildTests(unittest.TestCase):
    def test_final_smoke_uses_offscreen_qt_backend(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = root / "distribution"
            application = distribution / "datafuckerr"
            application.mkdir(parents=True)
            (application / "datafuckerr").write_text("application", encoding="utf-8")
            icons = root / "icons"
            icons.mkdir()
            (icons / "datafuckerr.png").write_bytes(b"icon")
            output = root / "output"
            output.mkdir()
            work = root / "work"
            tool = root / "appimagetool"
            tool.write_text("tool", encoding="utf-8")
            calls = []

            def fake_run(command, environment=None):
                calls.append((tuple(command), environment.copy()))
                if Path(command[0]).resolve() == tool.resolve():
                    Path(command[-1]).write_bytes(b"appimage")

            with (
                mock.patch.dict(os.environ, {"QT_QPA_PLATFORM": "xcb"}),
                mock.patch.object(build_native, "run", side_effect=fake_run),
            ):
                package = build_native.build_appimage(
                    distribution,
                    icons,
                    output,
                    work,
                    "1.2.3",
                    tool,
                )

            self.assertEqual(package.name, "datafuckerr-1.2.3-linux-x64.AppImage")
            self.assertEqual(calls[0][1]["QT_QPA_PLATFORM"], "xcb")
            self.assertEqual(calls[1][1]["QT_QPA_PLATFORM"], "offscreen")


if __name__ == "__main__":
    unittest.main()
