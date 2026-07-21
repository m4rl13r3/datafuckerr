#!/usr/bin/env python3

import argparse
import hashlib
import json
import re
import stat
import tarfile
import zipfile
from pathlib import Path, PurePosixPath


MAXIMUM_FILES = 512
MAXIMUM_FILE_SIZE = 128 * 1024 * 1024
MAXIMUM_TOTAL_SIZE = 256 * 1024 * 1024


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Vérifie un lot de publication diskpurge."
    )
    parser.add_argument("directory")
    return parser.parse_args()


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def exactly_one(values, label):
    if len(values) != 1:
        raise SystemExit(f"Le lot doit contenir exactement un fichier {label}.")
    return values[0]


def verify_checksums(directory, path):
    lines = path.read_text(encoding="ascii").splitlines()
    if len(lines) != 2:
        raise SystemExit("Le fichier de sommes doit contenir exactement deux entrées.")
    verified = set()
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
        if match is None:
            raise SystemExit("Le format du fichier de sommes SHA-256 est invalide.")
        expected, name = match.groups()
        candidate = directory / name
        if not candidate.is_file() or candidate.is_symlink():
            raise SystemExit(
                f"Le fichier référencé est absent ou non régulier : {name}"
            )
        if sha256_file(candidate) != expected:
            raise SystemExit(f"La somme SHA-256 ne correspond pas pour {name}.")
        verified.add(candidate)
    return verified


def safe_member(name):
    path = PurePosixPath(name)
    return not path.is_absolute() and ".." not in path.parts and "" not in path.parts


def read_tar(path):
    files = {}
    total_size = 0
    with tarfile.open(path, mode="r:gz") as archive:
        members = archive.getmembers()
        if len(members) > MAXIMUM_FILES:
            raise SystemExit("L’archive contient trop d’entrées.")
        for member in members:
            if not safe_member(member.name):
                raise SystemExit("L’archive contient un chemin dangereux.")
            if member.issym() or member.islnk() or member.isdev() or member.isfifo():
                raise SystemExit("L’archive contient un type d’entrée interdit.")
            if member.isfile():
                if member.size > MAXIMUM_FILE_SIZE:
                    raise SystemExit("L’archive contient un fichier trop volumineux.")
                total_size += member.size
                if total_size > MAXIMUM_TOTAL_SIZE:
                    raise SystemExit(
                        "La taille décompressée de l’archive est excessive."
                    )
                if member.name in files:
                    raise SystemExit("L’archive contient deux entrées de même nom.")
                stream = archive.extractfile(member)
                if stream is None:
                    raise SystemExit("Une entrée de l’archive ne peut pas être lue.")
                files[member.name] = stream.read()
    return files


def read_zip(path):
    files = {}
    total_size = 0
    with zipfile.ZipFile(path) as archive:
        members = archive.infolist()
        if len(members) > MAXIMUM_FILES:
            raise SystemExit("L’archive contient trop d’entrées.")
        for info in members:
            if not safe_member(info.filename):
                raise SystemExit("L’archive contient un chemin dangereux.")
            mode = info.external_attr >> 16
            if stat.S_ISLNK(mode):
                raise SystemExit("L’archive contient un lien symbolique interdit.")
            if not info.is_dir():
                if info.file_size > MAXIMUM_FILE_SIZE:
                    raise SystemExit("L’archive contient un fichier trop volumineux.")
                total_size += info.file_size
                if total_size > MAXIMUM_TOTAL_SIZE:
                    raise SystemExit(
                        "La taille décompressée de l’archive est excessive."
                    )
                if info.filename in files:
                    raise SystemExit("L’archive contient deux entrées de même nom.")
                files[info.filename] = archive.read(info)
    return files


def archive_files(path):
    if path.name.endswith(".tar.gz"):
        return read_tar(path)
    return read_zip(path)


def verify_archive(path, sbom):
    files = archive_files(path)
    if not files:
        raise SystemExit("L’archive est vide.")
    roots = {PurePosixPath(name).parts[0] for name in files}
    if len(roots) != 1:
        raise SystemExit("L’archive doit avoir une racine unique.")
    archive_root = next(iter(roots))
    required_names = ("VERSION", "README.md", "LICENSE", "manifest.json")
    for required_name in required_names:
        if f"{archive_root}/{required_name}" not in files:
            raise SystemExit(f"L’archive doit contenir {required_name} à sa racine.")
    binary_name = (
        f"{archive_root}/{'diskpurge.exe' if path.suffix == '.zip' else 'diskpurge'}"
    )
    if binary_name not in files:
        raise SystemExit("Le binaire est absent de la racine de l’archive.")
    manifest_name = f"{archive_root}/manifest.json"
    version_name = f"{archive_root}/VERSION"
    try:
        manifest = json.loads(files[manifest_name].decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise SystemExit("Le manifeste intégré est invalide.") from error
    version = files[version_name].decode("utf-8").strip()
    if manifest.get("version") != version:
        raise SystemExit("La version du manifeste intégré est incohérente.")
    if manifest.get("construction", {}).get("mode_laboratoire") is not False:
        raise SystemExit(
            "Le manifeste n’atteste pas la désactivation du mode laboratoire."
        )
    platform = manifest.get("plateforme")
    if not isinstance(platform, str) or not platform:
        raise SystemExit("La plateforme du manifeste intégré est invalide.")
    expected_root = f"diskpurge-{version}-{platform}"
    if roots != {expected_root} or not path.name.startswith(f"{expected_root}."):
        raise SystemExit("Le nom de l’archive ne correspond pas à son manifeste.")
    manifest_entries = manifest.get("fichiers", [])
    if not isinstance(manifest_entries, list):
        raise SystemExit("La liste de fichiers du manifeste intégré est invalide.")
    entries = {}
    for entry in manifest_entries:
        if not isinstance(entry, dict):
            raise SystemExit("Une entrée du manifeste intégré est invalide.")
        relative = entry.get("chemin")
        if not isinstance(relative, str) or not safe_member(relative):
            raise SystemExit("Un chemin du manifeste intégré est invalide.")
        if relative in entries:
            raise SystemExit("Le manifeste intégré contient un chemin dupliqué.")
        entries[relative] = entry
    archived_entries = {
        PurePosixPath(name).relative_to(expected_root).as_posix(): data
        for name, data in files.items()
        if not name.endswith("/manifest.json")
    }
    if set(entries) != set(archived_entries):
        raise SystemExit(
            "Le manifeste ne couvre pas exactement le contenu de l’archive."
        )
    for relative, data in archived_entries.items():
        entry = entries[relative]
        if entry.get("sha256") != hashlib.sha256(data).hexdigest():
            raise SystemExit(f"L’empreinte intégrée est incohérente pour {relative}.")
        if entry.get("taille") != len(data):
            raise SystemExit(f"La taille intégrée est incohérente pour {relative}.")
    binary_relative = PurePosixPath(binary_name).name
    binary_entry = entries.get(binary_relative)
    if binary_entry is None:
        raise SystemExit("Le binaire est absent du manifeste intégré.")
    if binary_entry.get("sha256") != hashlib.sha256(files[binary_name]).hexdigest():
        raise SystemExit("Le binaire ne correspond pas au manifeste intégré.")
    component = sbom.get("metadata", {}).get("component", {})
    if component.get("name") != "diskpurge" or component.get("version") != version:
        raise SystemExit("La nomenclature ne correspond pas à l’archive.")
    archive_components = [
        item for item in sbom.get("components", []) if item.get("name") == path.name
    ]
    if len(archive_components) != 1:
        raise SystemExit("La nomenclature ne référence pas exactement l’archive.")
    hashes = {
        item.get("alg"): item.get("content")
        for item in archive_components[0].get("hashes", [])
    }
    if hashes.get("SHA-256") != sha256_file(path):
        raise SystemExit(
            "La nomenclature contient une empreinte d’archive incohérente."
        )
    properties = {
        item.get("name"): item.get("value")
        for item in component.get("properties", [])
        if isinstance(item, dict)
    }
    source = manifest.get("source", {})
    if properties.get("diskpurge:sourceCommit") != source.get("commit"):
        raise SystemExit("Le commit de la nomenclature est incohérent.")
    if properties.get("diskpurge:platform") != platform:
        raise SystemExit("La plateforme de la nomenclature est incohérente.")
    binary_components = [
        item
        for item in sbom.get("components", [])
        if item.get("name") == binary_relative
    ]
    if len(binary_components) != 1:
        raise SystemExit("La nomenclature ne référence pas exactement le binaire.")
    binary_hashes = {
        item.get("alg"): item.get("content")
        for item in binary_components[0].get("hashes", [])
    }
    if binary_hashes.get("SHA-256") != hashlib.sha256(files[binary_name]).hexdigest():
        raise SystemExit(
            "La nomenclature contient une empreinte de binaire incohérente."
        )


def main():
    args = parse_arguments()
    candidate = Path(args.directory)
    if candidate.is_symlink():
        raise SystemExit(
            "Un lien symbolique est interdit comme répertoire de publication."
        )
    directory = candidate.resolve()
    if not directory.is_dir():
        raise SystemExit("Le répertoire de publication est absent ou non régulier.")
    archives = sorted(directory.glob("*.tar.gz")) + sorted(directory.glob("*.zip"))
    archive = exactly_one(archives, "d’archive")
    sbom_path = exactly_one(sorted(directory.glob("*.cdx.json")), "CycloneDX")
    checksums = exactly_one(sorted(directory.glob("*.sha256")), "de sommes SHA-256")
    if (
        sbom_path.stat().st_size > 16 * 1024 * 1024
        or checksums.stat().st_size > 64 * 1024
    ):
        raise SystemExit("Un fichier de métadonnées dépasse la taille autorisée.")
    verified = verify_checksums(directory, checksums)
    if verified != {archive, sbom_path}:
        raise SystemExit(
            "Le fichier de sommes ne couvre pas exactement l’archive et la nomenclature."
        )
    try:
        sbom = json.loads(sbom_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise SystemExit("La nomenclature CycloneDX est invalide.") from error
    if sbom.get("bomFormat") != "CycloneDX" or sbom.get("specVersion") != "1.5":
        raise SystemExit(
            "Le format de nomenclature CycloneDX n’est pas pris en charge."
        )
    verify_archive(archive, sbom)
    print(f"Lot vérifié : {archive.name}")


if __name__ == "__main__":
    main()
