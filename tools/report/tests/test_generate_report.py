import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).resolve().parents[1] / "generate_report.py"
MODULE_SPEC = importlib.util.spec_from_file_location("generate_report", MODULE_PATH)
REPORT_MODULE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(REPORT_MODULE)


def make_record(status, record_hash, previous_hash, terminal=False):
    values = {
        "schéma": 1,
        "horodatage": (
            "2026-07-21T08:30:00Z" if not terminal else "2026-07-21T08:31:00Z"
        ),
        "opération": "a" * 64,
        "opérateur": "Opératrice Démo",
        "témoin": "Témoin Démo",
        "version": "0.2.0",
        "périphérique": "image-virtuelle-test.img",
        "identifiant": "fichier-test-001",
        "identifiant_après": "fichier-test-001" if terminal else "",
        "taille_après": 4096 if terminal else 0,
        "identité_après_observée": terminal,
        "modèle": "Fichier virtuel",
        "firmware": "non applicable",
        "transport": "fichier",
        "environnement": "environnement de test",
        "topologie": "fichier régulier temporaire",
        "qualification": "VIRTUEL-TEST",
        "laboratoire": False,
        "taille": 4096,
        "taille_bloc": 1,
        "type_support": "fichier",
        "disque_entier": True,
        "identité_unique": True,
        "méthode_demandée": "clear-zero",
        "méthode_exécutée": "clear-zero",
        "sous_méthode_native": "aucune",
        "source_statut_natif": "aucune",
        "statut_natif_observé": False,
        "statut_natif_brut": 0,
        "vérification": "complète",
        "statut": status,
        "détail": "Fin de l’essai virtuel" if terminal else "Début de l’essai virtuel",
        "précédente": previous_hash,
        "empreinte": record_hash,
    }
    return {field: values[field] for field in REPORT_MODULE.AUDIT_FIELDS}


def valid_source():
    import json

    first_record = make_record("en_cours", "b" * 64, "0" * 64)
    final_record = make_record("réussi", "c" * 64, "b" * 64, terminal=True)
    source = (
        "\n".join(
            json.dumps(item, ensure_ascii=False, separators=(",", ":"))
            for item in (first_record, final_record)
        )
        + "\n"
    )
    return source.encode("utf-8")


def successful_verifier_result(record_count=2, record_hash="c" * 64):
    verifier_output = f"Journal valide : {record_count} enregistrement{'s' if record_count > 1 else ''}\nEmpreinte finale : {record_hash}\n"
    return subprocess.CompletedProcess([], 0, stdout=verifier_output, stderr="")


class ReportGenerationTests(unittest.TestCase):
    def test_verify_audit_runs_without_shell(self):
        with mock.patch.object(
            REPORT_MODULE.subprocess, "run", return_value=successful_verifier_result()
        ) as run_mock:
            result = REPORT_MODULE.verify_audit(
                Path("/opt/diskpurge"), Path("journal.jsonl")
            )
        self.assertEqual(result.record_count, 2)
        self.assertEqual(
            run_mock.call_args.args[0],
            ["/opt/diskpurge", "verify-audit", "journal.jsonl"],
        )
        self.assertIs(run_mock.call_args.kwargs["shell"], False)
        self.assertIs(run_mock.call_args.kwargs["check"], False)

    def test_verifier_failure_does_not_create_pdf(self):
        failure = subprocess.CompletedProcess(
            [], 2, stdout="", stderr="Journal invalide"
        )
        with tempfile.TemporaryDirectory() as directory:
            audit_log = Path(directory) / "journal.jsonl"
            output_path = Path(directory) / "rapport.pdf"
            audit_log.write_bytes(valid_source())
            with mock.patch.object(
                REPORT_MODULE.subprocess, "run", return_value=failure
            ):
                with self.assertRaises(REPORT_MODULE.ReportError):
                    REPORT_MODULE.generate_report(
                        audit_log, Path("diskpurge"), output_path
                    )
            self.assertFalse(output_path.exists())

    def test_verifier_runs_before_audit_log_is_read(self):
        failure = subprocess.CompletedProcess(
            [], 2, stdout="", stderr="Journal invalide"
        )
        with tempfile.TemporaryDirectory() as directory:
            missing_audit_log = Path(directory) / "absent.jsonl"
            output_path = Path(directory) / "rapport.pdf"
            with mock.patch.object(
                REPORT_MODULE.subprocess, "run", return_value=failure
            ) as run_mock:
                with self.assertRaisesRegex(
                    REPORT_MODULE.ReportError, "refusé par diskpurge"
                ):
                    REPORT_MODULE.generate_report(
                        missing_audit_log, Path("diskpurge"), output_path
                    )
            run_mock.assert_called_once()
            self.assertFalse(output_path.exists())

    def test_incomplete_operation_is_rejected_after_false_success(self):
        import json

        start = make_record("en_cours", "b" * 64, "0" * 64)
        source = (
            json.dumps(start, ensure_ascii=False, separators=(",", ":")) + "\n"
        ).encode("utf-8")
        verification = REPORT_MODULE.VerificationResult(1, "b" * 64, "Journal valide")
        with self.assertRaisesRegex(REPORT_MODULE.ReportError, "incomplète|terminal"):
            REPORT_MODULE.parse_jsonl(source, verification)

    def test_source_mismatch_is_rejected(self):
        verification = REPORT_MODULE.VerificationResult(2, "d" * 64, "Journal valide")
        with self.assertRaisesRegex(REPORT_MODULE.ReportError, "empreinte validée"):
            REPORT_MODULE.parse_jsonl(valid_source(), verification)

    def test_disclaimer_appears_on_every_pdf_page(self):
        if importlib.util.find_spec("reportlab") is None:
            self.skipTest("Dépendance PDF absente : reportlab")
        try:
            from pypdf import PdfReader
        except ImportError as error:
            self.skipTest(f"Dépendances PDF absentes : {error}")
        with tempfile.TemporaryDirectory() as directory:
            audit_log = Path(directory) / "journal.jsonl"
            output_path = Path(directory) / "rapport.pdf"
            audit_log.write_bytes(valid_source())
            with mock.patch.object(
                REPORT_MODULE.subprocess,
                "run",
                return_value=successful_verifier_result(),
            ):
                REPORT_MODULE.generate_report(
                    audit_log,
                    Path("diskpurge"),
                    output_path,
                    client="Client Exemple",
                    reference="DOSSIER-2026-001",
                )
            reader = PdfReader(str(output_path))
            self.assertGreaterEqual(len(reader.pages), 3)
            pages = [page.extract_text() or "" for page in reader.pages]
            for content in pages:
                self.assertIn(REPORT_MODULE.REPORT_DISCLAIMER, content)
            document = "\n".join(pages)
            self.assertIn("Aucune validation matérielle", document)
            self.assertIn("Aucun test indépendant", document)
            self.assertIn("locale, non signée", document)
            self.assertIn("utilisable uniquement comme trace d’exécution", document)
            self.assertNotIn("CERTIFICAT D’EFFACEMENT", document)


if __name__ == "__main__":
    unittest.main()
