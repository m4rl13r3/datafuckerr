import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from ui.diskpurge_commands import (
    assert_safe_command,
    build_erase_command,
    build_inspect_command,
    build_list_command,
    build_plan_command,
    build_verify_audit_command,
    execute_command,
    default_binary_path,
    inspection_signature,
    parse_inspection,
    plan_is_executable,
)


class CommandConstructionTests(unittest.TestCase):
    def test_frozen_application_uses_bundled_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary_name = "diskpurge.exe" if os.name == "nt" else "diskpurge"
            binary = Path(temporary) / binary_name
            binary.touch()
            with mock.patch(
                "ui.diskpurge_commands.sys._MEIPASS", temporary, create=True
            ):
                self.assertEqual(default_binary_path(), str(binary.resolve()))

    def test_list_command(self):
        self.assertEqual(
            build_list_command("/opt/disk purge"), ["/opt/disk purge", "list"]
        )

    def test_inspect_target_remains_one_argument(self):
        target = "/tmp/cible avec espaces;rm -rf"
        self.assertEqual(
            build_inspect_command("diskpurge", target),
            ["diskpurge", "inspect", target],
        )

    def test_plan_command(self):
        self.assertEqual(
            build_plan_command("diskpurge", "essai.img", "clear", "full"),
            [
                "diskpurge",
                "plan",
                "essai.img",
                "--method",
                "clear",
                "--verify",
                "full",
            ],
        )

    def test_verify_audit_command(self):
        self.assertEqual(
            build_verify_audit_command("diskpurge", "preuve opérateur.jsonl"),
            ["diskpurge", "verify-audit", "preuve opérateur.jsonl"],
        )

    def test_erase_command_contains_every_confirmation(self):
        command = build_erase_command(
            "diskpurge",
            "essai.img",
            "identifiant-stable",
            "clear",
            "full",
            "preuve.jsonl",
            "opérateur-01",
            "témoin-02",
            True,
        )
        self.assertEqual(command[0:3], ["diskpurge", "erase", "essai.img"])
        self.assertIn("--confirm", command)
        self.assertIn("--audit", command)
        self.assertIn("--operator", command)
        self.assertIn("--witness", command)
        self.assertIn("--acknowledge-data-loss", command)
        self.assertNotIn("--lab-mode", command)
        self.assertNotIn("--allow-internal", command)

    def test_erase_requires_acknowledgement(self):
        with self.assertRaisesRegex(ValueError, "acquittement"):
            build_erase_command(
                "diskpurge",
                "essai.img",
                "identifiant-stable",
                "clear",
                "full",
                "preuve.jsonl",
                "opérateur-01",
                "témoin-02",
                False,
            )

    def test_erase_requires_distinct_people(self):
        with self.assertRaisesRegex(ValueError, "différents"):
            build_erase_command(
                "diskpurge",
                "essai.img",
                "identifiant-stable",
                "clear",
                "full",
                "preuve.jsonl",
                "même-personne",
                "même-personne",
                True,
            )

    def test_erase_rejects_target_as_audit(self):
        target = os.path.abspath("essai.img")
        with self.assertRaisesRegex(ValueError, "distinct"):
            build_erase_command(
                "diskpurge",
                target,
                "identifiant-stable",
                "clear",
                "full",
                target,
                "opérateur-01",
                "témoin-02",
                True,
            )

    def test_invalid_choices_and_control_characters_are_rejected(self):
        with self.assertRaises(ValueError):
            build_plan_command("diskpurge", "essai.img", "commande inventée", "full")
        with self.assertRaises(ValueError):
            build_inspect_command("diskpurge", "cible\nseconde ligne")

    def test_physical_destruction_is_not_an_erase_method(self):
        with self.assertRaisesRegex(ValueError, "méthode"):
            build_erase_command(
                "diskpurge",
                "essai.img",
                "identifiant-stable",
                "destroy",
                "full",
                "preuve.jsonl",
                "opérateur-01",
                "témoin-02",
                True,
            )

    def test_lab_mode_is_rejected_centrally(self):
        with self.assertRaisesRegex(ValueError, "laboratoire"):
            assert_safe_command(["diskpurge", "erase", "essai.img", "--lab-mode"])

    def test_process_execution_explicitly_disables_shell(self):
        process = mock.Mock()
        process.communicate.return_value = ("résultat", None)
        process.returncode = 0
        with mock.patch(
            "ui.diskpurge_commands.subprocess.Popen", return_value=process
        ) as popen:
            self.assertEqual(execute_command(["diskpurge", "list"]), (0, "résultat"))
        arguments, options = popen.call_args
        self.assertEqual(arguments[0], ["diskpurge", "list"])
        self.assertIs(options["shell"], False)
        self.assertEqual(options["stdout"], subprocess.PIPE)
        self.assertEqual(options["stderr"], subprocess.STDOUT)


class InspectionParsingTests(unittest.TestCase):
    def test_inspection_identity_is_parsed_and_compared(self):
        output = "\n".join(
            [
                "Périphérique : /tmp/essai.img",
                "Identifiant  : fichier-123",
                "Modèle       : fichier régulier",
                "Firmware     : n/a",
                "Transport    : fichier",
                "Environnement : posix",
                "Topologie      : fichier",
                "Taille       : 4 KiB",
                "Support      : fichier",
                "Amovible     : non",
                "Interne      : non",
                "Monté        : non",
                "Utilisé      : non",
                "État de sûreté : établi",
                "Identité unique : oui",
                "Disque système : non",
                "Lecture seule  : non",
                "Qualification  : fichier-régulier",
            ]
        )
        first = parse_inspection(output)
        second = parse_inspection(output)
        self.assertEqual(first["Identifiant"], "fichier-123")
        self.assertEqual(inspection_signature(first), inspection_signature(second))

    def test_executable_plan_is_parsed_without_fixed_spacing(self):
        self.assertTrue(plan_is_executable("Exécutable : oui\n"))
        self.assertFalse(plan_is_executable("Exécutable   : non — refus\n"))
        self.assertFalse(plan_is_executable("Méthode : clear-zero\n"))

    def test_missing_identifier_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "identifiant"):
            parse_inspection("Modèle : inconnu")


if __name__ == "__main__":
    unittest.main()
