#!/usr/bin/env python3

import argparse
import datetime
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
import uuid
import zipfile
from pathlib import Path
from urllib.parse import quote


ROOT = Path(__file__).resolve().parents[2]
ROOT_FILES = (
    "VERSION",
    "README.md",
    "LICENSE",
    "CHANGELOG.md",
    "SECURITY.md",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
)
APPLICATION_FILES = (
    "ui/README.md",
    "ui/__init__.py",
    "ui/datafuckerr_qt.py",
    "ui/diskpurge_commands.py",
    "ui/requirements.txt",
    "ui/run_gui.cmd",
    "ui/run_gui.command",
    "ui/run_gui.sh",
    "tools/report/generate_report.py",
    "tools/report/requirements.txt",
)
EXECUTABLE_FILES = {
    "ui/datafuckerr_qt.py",
    "ui/run_gui.command",
    "ui/run_gui.sh",
    "tools/report/generate_report.py",
}


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Prépare une archive de publication diskpurge."
    )
    parser.add_argument("--binary", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--source-commit")
    parser.add_argument("--expected-ref", default="")
    return parser.parse_args()


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_value(*parts):
    process = subprocess.run(
        ("git", *parts),
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return process.stdout.strip()


def source_epoch(commit):
    raw = os.environ.get("SOURCE_DATE_EPOCH")
    if raw is None:
        raw = git_value("show", "-s", "--format=%ct", commit)
    if not raw.isdigit():
        raise SystemExit("SOURCE_DATE_EPOCH doit être un entier positif.")
    return int(raw)


def normalize_platform(value):
    normalized = value.strip().lower()
    if re.fullmatch(r"[a-z0-9][a-z0-9._-]*", normalized) is None:
        raise SystemExit("Le nom de plateforme contient un caractère interdit.")
    return normalized


def resolve_binary(value):
    path = Path(value)
    if not path.is_absolute():
        path = Path.cwd() / path
    if path.is_symlink():
        raise SystemExit(f"Un lien symbolique est interdit comme binaire : {path}")
    path = path.resolve()
    if not path.is_file():
        raise SystemExit(f"Le binaire est absent ou non régulier : {path}")
    return path


def read_version():
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
        raise SystemExit("VERSION ne contient pas une version sémantique stable.")
    return version


def verify_binary_version(binary, version):
    process = subprocess.run(
        (str(binary), "--version"),
        check=False,
        capture_output=True,
        text=True,
        timeout=10,
    )
    if process.returncode != 0 or process.stdout.strip() != version:
        raise SystemExit("La version du binaire ne correspond pas au fichier VERSION.")


def verify_release_ref(expected_ref, version):
    if expected_ref.startswith("refs/tags/"):
        tag = expected_ref.removeprefix("refs/tags/")
        if tag != f"v{version}":
            raise SystemExit(f"Le tag {tag} ne correspond pas à VERSION={version}.")


def resolve_commit(value):
    source = (
        value or os.environ.get("GITHUB_SHA") or git_value("rev-parse", "HEAD")
    ).lower()
    if re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", source) is None:
        raise SystemExit("L’identifiant du commit source est invalide.")
    commit = git_value("rev-parse", f"{source}^{{commit}}").lower()
    head = git_value("rev-parse", "HEAD").lower()
    if commit != head:
        raise SystemExit("Le commit déclaré ne correspond pas aux sources extraites.")
    return commit


def verify_repository(value):
    if re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", value) is None:
        raise SystemExit("Le nom du dépôt doit suivre la forme propriétaire/dépôt.")


def collect_documentation():
    paths = []
    for name in (*ROOT_FILES, *APPLICATION_FILES):
        path = ROOT / name
        if not path.is_file() or path.is_symlink():
            raise SystemExit(f"Le fichier requis est absent ou non régulier : {name}")
        paths.append(path)
    docs = ROOT / "docs"
    if not docs.is_dir() or docs.is_symlink():
        raise SystemExit("Le répertoire docs est absent ou non régulier.")
    for path in sorted(docs.rglob("*.md")):
        if path.is_symlink():
            raise SystemExit(
                f"Un lien symbolique est interdit dans la publication : {path}"
            )
        if path.is_file():
            paths.append(path)
    return paths


def stage_files(staging, binary, version, platform, repository, commit):
    root_name = f"diskpurge-{version}-{platform}"
    package_root = staging / root_name
    package_root.mkdir()
    binary_name = "diskpurge.exe" if binary.suffix.lower() == ".exe" else "diskpurge"
    staged_binary = package_root / binary_name
    shutil.copyfile(binary, staged_binary)
    staged_binary.chmod(0o755)
    for source in collect_documentation():
        relative = source.relative_to(ROOT)
        destination = package_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        destination.chmod(0o755 if relative.as_posix() in EXECUTABLE_FILES else 0o644)
    files = []
    for path in sorted(package_root.rglob("*")):
        if path.is_file():
            files.append(
                {
                    "chemin": path.relative_to(package_root).as_posix(),
                    "sha256": sha256_file(path),
                    "taille": path.stat().st_size,
                }
            )
    manifest = {
        "schéma": 1,
        "nom": "diskpurge",
        "version": version,
        "plateforme": platform,
        "source": {"dépôt": repository, "commit": commit},
        "construction": {"mode_laboratoire": False},
        "fichiers": files,
    }
    manifest_path = package_root / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    manifest_path.chmod(0o644)
    return package_root, staged_binary


def tar_info(archive, path, arcname, epoch):
    info = archive.gettarinfo(str(path), arcname=arcname)
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mtime = epoch
    info.pax_headers = {}
    return info


def create_tar(package_root, archive_path, epoch):
    with archive_path.open("wb") as raw:
        with gzip.GzipFile(
            filename="", mode="wb", fileobj=raw, mtime=epoch
        ) as compressed:
            with tarfile.open(
                fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT
            ) as archive:
                entries = (package_root, *sorted(package_root.rglob("*")))
                for path in entries:
                    arcname = path.relative_to(package_root.parent).as_posix()
                    info = tar_info(archive, path, arcname, epoch)
                    if path.is_file():
                        with path.open("rb") as stream:
                            archive.addfile(info, stream)
                    else:
                        archive.addfile(info)


def zip_datetime(epoch):
    minimum = 315532800
    value = max(epoch, minimum)
    return datetime.datetime.fromtimestamp(value, datetime.timezone.utc).timetuple()[:6]


def create_zip(package_root, archive_path, epoch):
    moment = zip_datetime(epoch)
    with zipfile.ZipFile(
        archive_path, mode="w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for path in sorted(package_root.rglob("*")):
            if not path.is_file():
                continue
            arcname = path.relative_to(package_root.parent).as_posix()
            info = zipfile.ZipInfo(arcname, date_time=moment)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            executable = path.name in ("diskpurge", "diskpurge.exe")
            info.external_attr = ((0o755 if executable else 0o644) & 0xFFFF) << 16
            archive.writestr(
                info,
                path.read_bytes(),
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9,
            )


def create_sbom(
    output, archive_path, binary_path, version, platform, repository, commit, epoch
):
    repository_path = quote(repository, safe="/")
    application_ref = f"pkg:github/{repository_path}@{version}"
    archive_ref = f"fichier:{archive_path.name}"
    binary_ref = f"fichier:{binary_path.name}"
    document = {
        "$schema": "http://cyclonedx.org/schema/bom-1.5.schema.json",
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, f'{repository}:{commit}:{platform}:{version}')}",
        "version": 1,
        "metadata": {
            "timestamp": datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc)
            .isoformat()
            .replace("+00:00", "Z"),
            "tools": [
                {
                    "vendor": "diskpurge",
                    "name": "package_release.py",
                    "version": version,
                }
            ],
            "component": {
                "type": "application",
                "bom-ref": application_ref,
                "name": "diskpurge",
                "version": version,
                "licenses": [{"license": {"id": "Apache-2.0"}}],
                "externalReferences": [
                    {
                        "type": "vcs",
                        "url": f"https://github.com/{repository}/tree/{commit}",
                    }
                ],
                "properties": [
                    {"name": "diskpurge:sourceCommit", "value": commit},
                    {"name": "diskpurge:platform", "value": platform},
                    {"name": "diskpurge:labMode", "value": "false"},
                ],
            },
        },
        "components": [
            {
                "type": "file",
                "bom-ref": binary_ref,
                "name": binary_path.name,
                "version": version,
                "hashes": [{"alg": "SHA-256", "content": sha256_file(binary_path)}],
                "properties": [{"name": "diskpurge:platform", "value": platform}],
            },
            {
                "type": "file",
                "bom-ref": archive_ref,
                "name": archive_path.name,
                "version": version,
                "hashes": [{"alg": "SHA-256", "content": sha256_file(archive_path)}],
            },
        ],
    }
    output.write_text(
        json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main():
    args = parse_arguments()
    version = read_version()
    platform = normalize_platform(args.platform)
    binary = resolve_binary(args.binary)
    commit = resolve_commit(args.source_commit)
    verify_repository(args.repository)
    verify_binary_version(binary, version)
    verify_release_ref(args.expected_ref, version)
    epoch = source_epoch(commit)
    output_candidate = Path(args.output_dir)
    if output_candidate.is_symlink():
        raise SystemExit("Un lien symbolique est interdit comme répertoire de sortie.")
    output_dir = output_candidate.resolve()
    if output_dir.exists():
        if not output_dir.is_dir() or output_dir.is_symlink():
            raise SystemExit("Le chemin de sortie est invalide.")
        if any(output_dir.iterdir()):
            raise SystemExit("Le répertoire de sortie doit être vide.")
    else:
        output_dir.mkdir(parents=True)
    base_name = f"diskpurge-{version}-{platform}"
    with tempfile.TemporaryDirectory(prefix="diskpurge-publication-") as temporary:
        package_root, staged_binary = stage_files(
            Path(temporary), binary, version, platform, args.repository, commit
        )
        if platform.startswith("windows-"):
            archive_path = output_dir / f"{base_name}.zip"
            create_zip(package_root, archive_path, epoch)
        else:
            archive_path = output_dir / f"{base_name}.tar.gz"
            create_tar(package_root, archive_path, epoch)
        sbom_path = output_dir / f"{base_name}.cdx.json"
        create_sbom(
            sbom_path,
            archive_path,
            staged_binary,
            version,
            platform,
            args.repository,
            commit,
            epoch,
        )
    checksums_path = output_dir / f"{base_name}.sha256"
    checksums_path.write_text(
        f"{sha256_file(archive_path)}  {archive_path.name}\n"
        f"{sha256_file(sbom_path)}  {sbom_path.name}\n",
        encoding="ascii",
    )
    print(
        json.dumps(
            {
                "archive": str(archive_path),
                "sbom": str(sbom_path),
                "sommes": str(checksums_path),
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
