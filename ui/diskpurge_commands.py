import os
import subprocess
import sys
from pathlib import Path


METHODS = ("auto", "clear", "purge")
PLAN_METHODS = ("auto", "clear", "purge", "destroy")
VERIFICATIONS = ("full", "sample")
INSPECTION_FIELDS = (
    "Périphérique",
    "Identifiant",
    "Modèle",
    "Firmware",
    "Transport",
    "Environnement",
    "Topologie",
    "Taille",
    "Support",
    "Amovible",
    "Interne",
    "Monté",
    "Utilisé",
    "État de sûreté",
    "Identité unique",
    "Disque système",
    "Lecture seule",
    "Qualification",
)


def validated_text(value, label):
    if not isinstance(value, str) or not value.strip():
        raise ValueError("{} est obligatoire.".format(label))
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError("{} contient un caractère de contrôle interdit.".format(label))
    return value


def validated_choice(value, choices, label):
    if value not in choices:
        raise ValueError("{} est invalide.".format(label))
    return value


def assert_safe_command(command):
    if not isinstance(command, list) or not command:
        raise ValueError("La commande doit être une liste non vide.")
    for argument in command:
        validated_text(argument, "Un argument")
    if "--lab-mode" in command:
        raise ValueError("Le mode laboratoire est interdit dans l’interface graphique.")
    return command


def build_version_command(binary):
    return assert_safe_command([validated_text(binary, "Le binaire"), "--version"])


def build_list_command(binary):
    return assert_safe_command([validated_text(binary, "Le binaire"), "list"])


def build_inspect_command(binary, target):
    return assert_safe_command(
        [
            validated_text(binary, "Le binaire"),
            "inspect",
            validated_text(target, "La cible"),
        ]
    )


def build_plan_command(binary, target, method="auto", verification="full"):
    return assert_safe_command(
        [
            validated_text(binary, "Le binaire"),
            "plan",
            validated_text(target, "La cible"),
            "--method",
            validated_choice(method, PLAN_METHODS, "La méthode"),
            "--verify",
            validated_choice(verification, VERIFICATIONS, "La vérification"),
        ]
    )


def build_verify_audit_command(binary, audit_path):
    return assert_safe_command(
        [
            validated_text(binary, "Le binaire"),
            "verify-audit",
            validated_text(audit_path, "Le journal d’audit"),
        ]
    )


def build_erase_command(
    binary,
    target,
    identifier,
    method,
    verification,
    audit_path,
    operator,
    witness,
    acknowledged,
):
    if not acknowledged:
        raise ValueError(
            "L’acquittement explicite de la perte de données est obligatoire."
        )
    operator = validated_text(operator, "L’identifiant de l’opérateur")
    witness = validated_text(witness, "L’identifiant du témoin")
    if operator == witness:
        raise ValueError("L’opérateur et le témoin doivent être différents.")
    target = validated_text(target, "La cible")
    audit_path = validated_text(audit_path, "Le journal d’audit")
    if os.path.normcase(os.path.abspath(target)) == os.path.normcase(
        os.path.abspath(audit_path)
    ):
        raise ValueError("Le journal d’audit doit être distinct de la cible.")
    return assert_safe_command(
        [
            validated_text(binary, "Le binaire"),
            "erase",
            target,
            "--confirm",
            validated_text(identifier, "L’identifiant recopié"),
            "--method",
            validated_choice(method, METHODS, "La méthode"),
            "--verify",
            validated_choice(verification, VERIFICATIONS, "La vérification"),
            "--audit",
            audit_path,
            "--operator",
            operator,
            "--witness",
            witness,
            "--acknowledge-data-loss",
        ]
    )


def parse_inspection(output):
    fields = {}
    for line in output.splitlines():
        if ":" not in line:
            continue
        name, value = line.split(":", 1)
        name = name.strip()
        if name in INSPECTION_FIELDS:
            fields[name] = value.strip()
    if not fields.get("Identifiant"):
        raise ValueError("L’inspection ne contient pas d’identifiant stable.")
    missing = [name for name in INSPECTION_FIELDS if name not in fields]
    if missing:
        raise ValueError("L’inspection est incomplète : {}.".format(", ".join(missing)))
    return fields


def inspection_signature(fields):
    return tuple((name, fields.get(name, "")) for name in INSPECTION_FIELDS)


def plan_is_executable(output):
    for line in output.splitlines():
        if ":" not in line:
            continue
        name, value = line.split(":", 1)
        if name.strip() == "Exécutable":
            return value.strip() == "oui"
    return False


def application_root():
    bundled_root = getattr(sys, "_MEIPASS", "")
    if bundled_root:
        return Path(bundled_root).resolve()
    return Path(__file__).resolve().parent.parent


def default_binary_path():
    root = application_root()
    names = []
    if os.name == "nt":
        names.extend(
            [
                root / "diskpurge.exe",
                root / "build" / "diskpurge.exe",
                root / "build-cmake" / "Release" / "diskpurge.exe",
                root / "build-cmake" / "diskpurge.exe",
            ]
        )
    else:
        names.extend(
            [
                root / "diskpurge",
                root / "build" / "diskpurge",
                root / "build-cmake" / "diskpurge",
            ]
        )
    for candidate in names:
        if candidate.is_file():
            return str(candidate)
    return str(names[0])


def execute_command(command):
    assert_safe_command(command)
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=False,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output, unused = process.communicate()
    return process.returncode, output


def displayed_command(command):
    return " ".join(repr(argument) for argument in command)
