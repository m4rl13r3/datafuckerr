import os
import sys
from pathlib import Path


root = Path(SPECPATH).parents[1]
icon_directory = Path(os.environ["DATAFUCKERR_ICON_DIRECTORY"])
license_directory = Path(os.environ["DATAFUCKERR_LICENSE_DIRECTORY"])
binary = Path(os.environ["DATAFUCKERR_BINARY"])
version = (root / "VERSION").read_text(encoding="utf-8").strip()
icon = icon_directory / (
    "datafuckerr.icns"
    if sys.platform == "darwin"
    else "datafuckerr.ico"
    if sys.platform == "win32"
    else "datafuckerr.png"
)

analysis = Analysis(
    [str(root / "ui" / "datafuckerr_qt.py")],
    pathex=[str(root), str(root / "ui")],
    binaries=[(str(binary), ".")],
    datas=[
        (str(root / "VERSION"), "."),
        (str(root / "LICENSE"), "."),
        (str(root / "packaging" / "native" / "datafuckerr.svg"), "."),
        (str(root / "packaging" / "native" / "THIRD_PARTY_NOTICES.md"), "."),
        (str(license_directory), "licenses"),
    ],
    hiddenimports=["tools.report.generate_report"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=1,
)
python_archive = PYZ(analysis.pure)
executable = EXE(
    python_archive,
    analysis.scripts,
    [],
    exclude_binaries=True,
    name="datafuckerr",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    icon=str(icon),
)
collection = COLLECT(
    executable,
    analysis.binaries,
    analysis.datas,
    strip=False,
    upx=False,
    name="datafuckerr",
)

if sys.platform == "darwin":
    application = BUNDLE(
        collection,
        name="datafuckerr.app",
        icon=str(icon),
        bundle_identifier="fr.datafuckerr.desktop",
        version=version,
        info_plist={
            "CFBundleDisplayName": "datafuckerr",
            "CFBundleName": "datafuckerr",
            "CFBundleShortVersionString": version,
            "CFBundleVersion": version,
            "LSMinimumSystemVersion": "13.0",
            "NSHighResolutionCapable": True,
        },
    )
