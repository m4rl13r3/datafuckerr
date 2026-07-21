#!/usr/bin/env python3

import argparse
import hashlib
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAXIMUM_PACKAGE_SIZE = 1024 * 1024 * 1024


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def package_signature(path):
    with path.open("rb") as stream:
        start = stream.read(4)
        if path.suffix == ".dmg":
            stream.seek(-512, 2)
            end = stream.read(4)
        else:
            end = b""
    if path.suffix == ".AppImage" and start != b"\x7fELF":
        raise SystemExit("L’AppImage ne possède pas une signature ELF valide.")
    if path.suffix == ".exe" and start[:2] != b"MZ":
        raise SystemExit(
            "L’installateur Windows ne possède pas une signature PE valide."
        )
    if path.suffix == ".dmg" and end != b"koly":
        raise SystemExit("L’image macOS ne possède pas une structure UDIF valide.")


def main():
    parser = argparse.ArgumentParser(
        description="Vérifie un paquet d’application native datafuckerr."
    )
    parser.add_argument("directory")
    args = parser.parse_args()
    candidate = Path(args.directory)
    if candidate.is_symlink():
        raise SystemExit("Un lien symbolique est interdit comme répertoire natif.")
    directory = candidate.resolve()
    if not directory.is_dir():
        raise SystemExit("Le répertoire natif est absent.")
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    files = sorted(path for path in directory.iterdir() if path.is_file())
    if len(files) != 2:
        raise SystemExit("Le lot natif doit contenir exactement deux fichiers.")
    checksums = [path for path in files if path.name.endswith(".sha256")]
    packages = [path for path in files if not path.name.endswith(".sha256")]
    if len(checksums) != 1 or len(packages) != 1:
        raise SystemExit("Le lot natif doit contenir une application et sa somme.")
    package = packages[0]
    checksum = checksums[0]
    allowed_names = {
        f"datafuckerr-{version}-linux-x64.AppImage",
        f"datafuckerr-{version}-macos-arm64.dmg",
        f"datafuckerr-{version}-windows-x64-setup.exe",
    }
    if package.name not in allowed_names or checksum.name != f"{package.name}.sha256":
        raise SystemExit("Le nom du paquet natif est invalide.")
    if package.stat().st_size <= 0 or package.stat().st_size > MAXIMUM_PACKAGE_SIZE:
        raise SystemExit("La taille du paquet natif est invalide.")
    line = checksum.read_text(encoding="ascii").strip()
    match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
    if match is None or match.group(2) != package.name:
        raise SystemExit("Le fichier de somme natif est invalide.")
    if match.group(1) != sha256_file(package):
        raise SystemExit("La somme du paquet natif ne correspond pas.")
    package_signature(package)
    print(f"Application native vérifiée : {package.name}")


if __name__ == "__main__":
    main()
