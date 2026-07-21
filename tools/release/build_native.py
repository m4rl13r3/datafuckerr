#!/usr/bin/env python3

import argparse
import hashlib
import importlib.metadata
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LICENSE_FILES = (
    (
        "GPL-3.0-only.txt",
        "fb981668c18a279e285fc4d83fba1e836cc84dd4daa73c9697d3cfd2d8aca6e0",
    ),
    (
        "LGPL-3.0-only.txt",
        "996af0513df21f7496288951c41428a03c174e9e4a9d63665c57d670f845ccb1",
    ),
    (
        "PSF-2.0.txt",
        "ab745c5061d1dea43a3885e5b4b6befc7e983954954775c5736debeefcdfd89b",
    ),
)
PACKAGE_LICENSES = (
    ("charset-normalizer", "charset-normalizer-LICENSE.txt"),
    ("Pillow", "Pillow-LICENSE.txt"),
    ("PyInstaller", "PyInstaller-COPYING.txt"),
    ("reportlab", "ReportLab-LICENSE.txt"),
)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Construit une application native autonome datafuckerr."
    )
    parser.add_argument("--binary", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--work-dir", required=True)
    parser.add_argument("--appimagetool")
    parser.add_argument("--inno-compiler")
    return parser.parse_args()


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_version():
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
        raise SystemExit("VERSION ne contient pas une version sémantique stable.")
    return version


def normalize_current_platform():
    system_names = {"linux": "linux", "darwin": "macos", "win32": "windows"}
    system = system_names.get(sys.platform)
    if system is None:
        raise SystemExit("La plateforme de construction n’est pas prise en charge.")
    machine = platform.machine().lower()
    architecture_names = {
        "amd64": "x64",
        "x86_64": "x64",
        "arm64": "arm64",
        "aarch64": "arm64",
    }
    architecture = architecture_names.get(machine)
    if architecture is None:
        raise SystemExit(f"Architecture non prise en charge : {machine}")
    return f"{system}-{architecture}"


def resolve_regular_file(value, label):
    candidate = Path(value).expanduser()
    if candidate.is_symlink():
        raise SystemExit(f"Un lien symbolique est interdit pour {label}.")
    path = candidate.resolve()
    if not path.is_file():
        raise SystemExit(f"Le fichier requis est absent pour {label} : {path}")
    return path


def prepare_empty_directory(value, label):
    candidate = Path(value).expanduser()
    if candidate.is_symlink():
        raise SystemExit(f"Un lien symbolique est interdit pour {label}.")
    path = candidate.resolve()
    if path.exists():
        if not path.is_dir() or any(path.iterdir()):
            raise SystemExit(f"Le répertoire {label} doit être vide.")
    else:
        path.mkdir(parents=True)
    return path


def run(command, environment=None):
    subprocess.run(command, cwd=ROOT, env=environment, check=True)


def verify_binary(binary, version):
    result = subprocess.run(
        (str(binary), "--version"),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=10,
        check=False,
        shell=False,
    )
    if result.returncode != 0 or result.stdout.strip() != version:
        raise SystemExit("Le moteur standard ne correspond pas à VERSION.")


def prepare_licenses(work):
    directory = work / "licenses"
    directory.mkdir()
    source_directory = ROOT / "packaging" / "native" / "licenses"
    for name, expected in LICENSE_FILES:
        source = source_directory / name
        if source.is_symlink() or not source.is_file():
            raise SystemExit(f"Le texte de licence {name} est absent.")
        content = source.read_bytes()
        if len(content) > 1024 * 1024:
            raise SystemExit(f"Le texte de licence {name} est trop volumineux.")
        if hashlib.sha256(content).hexdigest() != expected:
            raise SystemExit(f"La somme du texte de licence {name} est invalide.")
        shutil.copyfile(source, directory / name)
    for distribution_name, output_name in PACKAGE_LICENSES:
        distribution = importlib.metadata.distribution(distribution_name)
        candidates = [
            item
            for item in distribution.files or ()
            if item.name.lower().startswith(("license", "copying"))
        ]
        if len(candidates) != 1:
            raise SystemExit(
                f"La licence du paquet {distribution_name} est introuvable."
            )
        source = Path(distribution.locate_file(candidates[0])).resolve()
        shutil.copyfile(source, directory / output_name)
    return directory


def build_pyinstaller(binary, work, platform_name):
    icon_directory = work / "icons"
    run(
        (
            sys.executable,
            str(ROOT / "packaging" / "native" / "render_icons.py"),
            str(icon_directory),
        )
    )
    license_directory = prepare_licenses(work)
    distribution = work / "dist"
    environment = os.environ.copy()
    environment["DATAFUCKERR_BINARY"] = str(binary)
    environment["DATAFUCKERR_ICON_DIRECTORY"] = str(icon_directory)
    environment["DATAFUCKERR_LICENSE_DIRECTORY"] = str(license_directory)
    run(
        (
            sys.executable,
            "-m",
            "PyInstaller",
            "--noconfirm",
            "--clean",
            "--distpath",
            str(distribution),
            "--workpath",
            str(work / "pyinstaller"),
            str(ROOT / "packaging" / "native" / "datafuckerr.spec"),
        ),
        environment,
    )
    executable_name = (
        "datafuckerr.exe" if platform_name.startswith("windows-") else "datafuckerr"
    )
    executable = distribution / "datafuckerr" / executable_name
    if platform_name.startswith("macos-"):
        executable = (
            distribution / "datafuckerr.app" / "Contents" / "MacOS" / "datafuckerr"
        )
    if not executable.is_file():
        raise SystemExit("L’exécutable graphique PyInstaller est absent.")
    smoke_environment = os.environ.copy()
    smoke_environment["QT_QPA_PLATFORM"] = "offscreen"
    run((str(executable), "--native-smoke-test"), smoke_environment)
    return distribution, icon_directory


def build_appimage(distribution, icon_directory, output, work, version, appimagetool):
    tool = resolve_regular_file(appimagetool, "appimagetool")
    app_directory = work / "datafuckerr.AppDir"
    application_directory = app_directory / "usr" / "lib" / "datafuckerr"
    application_directory.parent.mkdir(parents=True)
    shutil.copytree(distribution / "datafuckerr", application_directory, symlinks=True)
    binary_directory = app_directory / "usr" / "bin"
    binary_directory.mkdir(parents=True)
    os.symlink("../lib/datafuckerr/datafuckerr", binary_directory / "datafuckerr")
    app_run = app_directory / "AppRun"
    app_run.write_text(
        '#!/bin/sh\nset -eu\napp_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)\nexec "$app_directory/usr/bin/datafuckerr" "$@"\n',
        encoding="utf-8",
    )
    app_run.chmod(0o755)
    shutil.copyfile(
        ROOT / "packaging" / "native" / "datafuckerr.desktop",
        app_directory / "datafuckerr.desktop",
    )
    shutil.copyfile(
        icon_directory / "datafuckerr.png", app_directory / "datafuckerr.png"
    )
    os.symlink("datafuckerr.png", app_directory / ".DirIcon")
    package = output / f"datafuckerr-{version}-linux-x64.AppImage"
    environment = os.environ.copy()
    environment["APPIMAGE_EXTRACT_AND_RUN"] = "1"
    environment["ARCH"] = "x86_64"
    run((str(tool), "--no-appstream", str(app_directory), str(package)), environment)
    package.chmod(0o755)
    run((str(package), "--native-smoke-test"), environment)
    return package


def build_dmg(distribution, output, work, version):
    application = distribution / "datafuckerr.app"
    run(
        (
            "codesign",
            "--force",
            "--deep",
            "--sign",
            "-",
            "--timestamp=none",
            str(application),
        )
    )
    staging = work / "dmg"
    staging.mkdir()
    shutil.copytree(application, staging / "datafuckerr.app", symlinks=True)
    os.symlink("/Applications", staging / "Applications")
    package = output / f"datafuckerr-{version}-macos-arm64.dmg"
    run(
        (
            "hdiutil",
            "create",
            "-volname",
            f"datafuckerr {version}",
            "-srcfolder",
            str(staging),
            "-format",
            "UDZO",
            "-ov",
            str(package),
        )
    )
    return package


def build_windows_installer(distribution, icon_directory, output, version, compiler):
    tool = resolve_regular_file(compiler, "le compilateur Inno Setup")
    output_name = f"datafuckerr-{version}-windows-x64-setup"
    run(
        (
            str(tool),
            "/Qp",
            f"/DAppVersion={version}",
            f"/DSourceDirectory={distribution / 'datafuckerr'}",
            f"/DOutputDirectory={output}",
            f"/DOutputBaseName={output_name}",
            f"/DIconFile={icon_directory / 'datafuckerr.ico'}",
            f"/DLicenseFile={ROOT / 'LICENSE'}",
            str(ROOT / "packaging" / "native" / "datafuckerr.iss"),
        )
    )
    package = output / f"{output_name}.exe"
    if not package.is_file():
        raise SystemExit("L’installateur Windows est absent.")
    return package


def write_checksum(package):
    checksum = package.with_name(f"{package.name}.sha256")
    checksum.write_text(f"{sha256_file(package)}  {package.name}\n", encoding="ascii")
    return checksum


def main():
    args = parse_arguments()
    version = read_version()
    platform_name = normalize_current_platform()
    if args.platform.lower() != platform_name:
        raise SystemExit(
            f"La plateforme déclarée {args.platform} ne correspond pas à {platform_name}."
        )
    binary = resolve_regular_file(args.binary, "le moteur diskpurge")
    verify_binary(binary, version)
    output = prepare_empty_directory(args.output_dir, "de sortie")
    work = prepare_empty_directory(args.work_dir, "de construction")
    distribution, icon_directory = build_pyinstaller(binary, work, platform_name)
    if platform_name == "linux-x64":
        if not args.appimagetool:
            raise SystemExit("appimagetool est obligatoire sous Linux.")
        package = build_appimage(
            distribution,
            icon_directory,
            output,
            work,
            version,
            args.appimagetool,
        )
    elif platform_name == "macos-arm64":
        package = build_dmg(distribution, output, work, version)
    elif platform_name == "windows-x64":
        if not args.inno_compiler:
            raise SystemExit("Le compilateur Inno Setup est obligatoire sous Windows.")
        package = build_windows_installer(
            distribution,
            icon_directory,
            output,
            version,
            args.inno_compiler,
        )
    else:
        raise SystemExit(f"Le paquet natif {platform_name} n’est pas pris en charge.")
    checksum = write_checksum(package)
    print(
        json.dumps(
            {"application": str(package), "somme": str(checksum)},
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
