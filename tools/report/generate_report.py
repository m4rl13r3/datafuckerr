import argparse
import hashlib
import html
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

REPORT_DISCLAIMER = "RAPPORT TECHNIQUE NON CERTIFIÉ"
MAX_AUDIT_SIZE = 128 * 1024 * 1024
AUDIT_FIELDS = (
    "schéma",
    "horodatage",
    "opération",
    "opérateur",
    "témoin",
    "version",
    "périphérique",
    "identifiant",
    "identifiant_après",
    "taille_après",
    "identité_après_observée",
    "modèle",
    "firmware",
    "transport",
    "environnement",
    "topologie",
    "qualification",
    "laboratoire",
    "taille",
    "taille_bloc",
    "type_support",
    "disque_entier",
    "identité_unique",
    "méthode_demandée",
    "méthode_exécutée",
    "sous_méthode_native",
    "source_statut_natif",
    "statut_natif_observé",
    "statut_natif_brut",
    "vérification",
    "statut",
    "détail",
    "précédente",
    "empreinte",
)
INTEGER_FIELDS = {
    "schéma",
    "taille_après",
    "taille",
    "taille_bloc",
    "statut_natif_brut",
}
BOOLEAN_FIELDS = {
    "identité_après_observée",
    "laboratoire",
    "disque_entier",
    "identité_unique",
    "statut_natif_observé",
}
TERMINAL_STATUSES = {"refusé", "réussi", "échoué", "indéterminé"}
VERIFIER_OUTPUT_PATTERN = re.compile(
    r"\AJournal valide : ([1-9][0-9]*) enregistrement(?:s)?\r?\n"
    r"Empreinte finale : ([0-9a-f]{64})\r?\n?\Z"
)


class ReportError(Exception):
    pass


@dataclass(frozen=True)
class VerificationResult:
    record_count: int
    final_hash: str
    verifier_output: str


@dataclass(frozen=True)
class AuditOperation:
    start: dict
    end: dict


def find_binary(value, repo_root):
    if value:
        path = Path(value).expanduser()
        if path.parent != Path(".") or path.exists():
            return path.absolute()
        found = shutil.which(value)
        if found:
            return Path(found)
        raise ReportError(f"Binaire diskpurge introuvable : {value}")
    names = ("diskpurge.exe", "diskpurge") if os.name == "nt" else ("diskpurge",)
    for name in names:
        candidate = repo_root / "build" / name
        if candidate.is_file():
            return candidate
    found = shutil.which("diskpurge")
    if found:
        return Path(found)
    raise ReportError("Binaire diskpurge introuvable. Utilisez --diskpurge.")


def verify_audit(binary, audit_log):
    command = [str(binary), "verify-audit", str(audit_log)]
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
            check=False,
            shell=False,
        )
    except subprocess.TimeoutExpired as error:
        raise ReportError(
            "La vérification du journal a dépassé 30 secondes."
        ) from error
    except OSError as error:
        raise ReportError(f"Impossible d’exécuter diskpurge : {error}") from error
    if result.returncode != 0:
        diagnostic = (result.stderr or result.stdout).strip()
        suffix = f" Détail : {diagnostic}" if diagnostic else ""
        raise ReportError(f"Journal refusé par diskpurge verify-audit.{suffix}")
    match = VERIFIER_OUTPUT_PATTERN.fullmatch(result.stdout)
    if match is None:
        raise ReportError(
            "Sortie inattendue de diskpurge verify-audit ; aucun rapport n’a été créé."
        )
    return VerificationResult(
        record_count=int(match.group(1)),
        final_hash=match.group(2),
        verifier_output=result.stdout.strip(),
    )


def read_source(audit_log):
    flags = os.O_RDONLY
    flags |= getattr(os, "O_BINARY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        file_descriptor = os.open(str(audit_log), flags)
    except OSError as error:
        raise ReportError(f"Impossible de lire le journal vérifié : {error}") from error
    try:
        before = os.fstat(file_descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1:
            raise ReportError(
                "Le journal vérifié n’est pas un fichier régulier unique."
            )
        if before.st_size <= 0:
            raise ReportError("Le journal vérifié est vide.")
        if before.st_size > MAX_AUDIT_SIZE:
            raise ReportError("Le journal dépasse la taille maximale prise en charge.")
        chunks = []
        total = 0
        while True:
            chunk = os.read(file_descriptor, 1024 * 1024)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_AUDIT_SIZE:
                raise ReportError("Le journal a grandi pendant sa lecture.")
            chunks.append(chunk)
        after = os.fstat(file_descriptor)
        signature_before = (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
        )
        signature_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
        if signature_before != signature_after or total != after.st_size:
            raise ReportError("Le journal a changé pendant sa lecture.")
        return b"".join(chunks)
    finally:
        os.close(file_descriptor)


def validate_record_types(record, line_number):
    if tuple(record.keys()) != AUDIT_FIELDS:
        raise ReportError(f"Schéma JSONL inattendu à la ligne {line_number}.")
    for field in AUDIT_FIELDS:
        value = record[field]
        if field in INTEGER_FIELDS:
            valid = type(value) is int and value >= 0
        elif field in BOOLEAN_FIELDS:
            valid = type(value) is bool
        else:
            valid = type(value) is str
        if not valid:
            raise ReportError(
                f"Type invalide pour « {field} » à la ligne {line_number}."
            )
    if record["schéma"] != 1:
        raise ReportError(
            f"Version de schéma non prise en charge à la ligne {line_number}."
        )
    if record["statut"] not in TERMINAL_STATUSES | {"en_cours"}:
        raise ReportError(f"Statut inconnu à la ligne {line_number}.")
    for field in ("opération", "précédente", "empreinte"):
        if re.fullmatch(r"[0-9a-f]{64}", record[field]) is None:
            raise ReportError(
                f"Empreinte ou identifiant invalide à la ligne {line_number}."
            )


def group_operations(records):
    groups = {}
    order = []
    active_operation_id = None
    for line_number, record in enumerate(records, 1):
        operation_id = record["opération"]
        raw_status = record["statut"]
        existing_records = groups.get(operation_id)
        if existing_records is None:
            if raw_status == "refusé":
                groups[operation_id] = [record]
                order.append(operation_id)
                continue
            if raw_status != "en_cours" or active_operation_id is not None:
                raise ReportError(
                    f"Opération sans démarrage valide à la ligne {line_number}."
                )
            groups[operation_id] = [record]
            order.append(operation_id)
            active_operation_id = operation_id
            continue
        if len(existing_records) != 1 or existing_records[0]["statut"] != "en_cours":
            raise ReportError(f"Transition dupliquée à la ligne {line_number}.")
        if (
            raw_status not in TERMINAL_STATUSES - {"refusé"}
            or active_operation_id != operation_id
        ):
            raise ReportError(f"État terminal incohérent à la ligne {line_number}.")
        existing_records.append(record)
        active_operation_id = None
    operations = []
    for operation_id in order:
        entries = groups[operation_id]
        if entries[0]["statut"] == "en_cours" and len(entries) != 2:
            raise ReportError(f"L’opération {operation_id} est incomplète.")
        end = entries[-1]
        if end["statut"] not in TERMINAL_STATUSES:
            raise ReportError(
                f"L’opération {operation_id} ne possède pas d’état terminal."
            )
        operations.append(AuditOperation(start=entries[0], end=end))
    if active_operation_id is not None or not operations:
        raise ReportError("Le journal ne contient aucune opération complète.")
    return operations


def parse_jsonl(source, verification):
    if not source.endswith(b"\n"):
        raise ReportError("Le journal est tronqué : saut de ligne final absent.")
    try:
        decoded_text = source.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise ReportError("Le journal n’est pas un document UTF-8 valide.") from error
    lines = decoded_text[:-1].split("\n")
    if not lines or any(line == "" for line in lines):
        raise ReportError(
            "Le journal contient une ligne vide ou ne contient aucun enregistrement."
        )
    records = []
    for line_number, line in enumerate(lines, 1):
        try:
            record = json.loads(line)
        except json.JSONDecodeError as error:
            raise ReportError(f"JSON invalide à la ligne {line_number}.") from error
        if type(record) is not dict:
            raise ReportError(f"Objet JSON attendu à la ligne {line_number}.")
        validate_record_types(record, line_number)
        records.append(record)
    if len(records) != verification.record_count:
        raise ReportError(
            "Le journal analysé ne correspond pas au nombre validé par diskpurge."
        )
    if records[-1]["empreinte"] != verification.final_hash:
        raise ReportError(
            "Le journal analysé ne correspond pas à l’empreinte validée par diskpurge."
        )
    operations = group_operations(records)
    return records, operations


def format_value(value):
    if value is True:
        return "oui"
    if value is False:
        return "non"
    if value == "":
        return "non renseigné"
    return str(value)


def human_size(value):
    units = ("o", "Kio", "Mio", "Gio", "Tio", "Pio")
    quantity = float(value)
    index = 0
    while quantity >= 1024 and index < len(units) - 1:
        quantity /= 1024
        index += 1
    if index == 0:
        return f"{int(quantity)} {units[index]}"
    return f"{quantity:.2f} {units[index]}"


def create_pdf(
    output_path, audit_log, binary, source, verification, operations, client, reference
):
    try:
        from reportlab.lib import colors
        from reportlab.lib.enums import TA_CENTER, TA_LEFT
        from reportlab.lib.pagesizes import A4
        from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
        from reportlab.lib.units import mm
        from reportlab.platypus import (
            BaseDocTemplate,
            Frame,
            KeepTogether,
            LongTable,
            PageBreak,
            PageTemplate,
            Paragraph,
            Spacer,
            Table,
            TableStyle,
        )
    except ImportError as error:
        raise ReportError(
            "La dépendance reportlab est absente. Installez tools/report/requirements.txt."
        ) from error

    navy = colors.HexColor("#17324D")
    pale_blue = colors.HexColor("#EAF1F7")
    red = colors.HexColor("#A3262A")
    pale_red = colors.HexColor("#F9E9E9")
    gray = colors.HexColor("#56606A")
    border = colors.HexColor("#CAD2D9")
    background = colors.HexColor("#F6F8FA")
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="ReportTitle",
            parent=styles["Title"],
            fontName="Helvetica-Bold",
            fontSize=23,
            leading=27,
            textColor=navy,
            alignment=TA_LEFT,
            spaceAfter=8 * mm,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportSubtitle",
            parent=styles["Normal"],
            fontName="Helvetica",
            fontSize=10.5,
            leading=15,
            textColor=gray,
            spaceAfter=5 * mm,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportSection",
            parent=styles["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=14,
            leading=18,
            textColor=navy,
            spaceBefore=5 * mm,
            spaceAfter=3 * mm,
            keepWithNext=True,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportBody",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=9.5,
            leading=14,
            textColor=colors.HexColor("#20262C"),
            spaceAfter=2.5 * mm,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportCell",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=8.2,
            leading=11,
            textColor=colors.HexColor("#20262C"),
            wordWrap="CJK",
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportKeyCell",
            parent=styles["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=8.2,
            leading=11,
            textColor=navy,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportHeaderCell",
            parent=styles["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=8.2,
            leading=11,
            textColor=colors.white,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportAlert",
            parent=styles["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=10.5,
            leading=15,
            textColor=red,
            alignment=TA_CENTER,
        )
    )
    styles.add(
        ParagraphStyle(
            name="ReportSmall",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=7.5,
            leading=10,
            textColor=gray,
            wordWrap="CJK",
        )
    )

    def escaped(value):
        return html.escape(format_value(value), quote=True)

    def paragraph(value, style="ReportBody"):
        return Paragraph(escaped(value), styles[style])

    def key_value_table(lines):
        rows = [
            [
                Paragraph(escaped(key), styles["ReportKeyCell"]),
                Paragraph(escaped(value), styles["ReportCell"]),
            ]
            for key, value in lines
        ]
        table = LongTable(
            rows, colWidths=[46 * mm, 118 * mm], repeatRows=0, hAlign="LEFT"
        )
        table.setStyle(
            TableStyle(
                [
                    ("BACKGROUND", (0, 0), (0, -1), pale_blue),
                    ("BACKGROUND", (1, 0), (1, -1), colors.white),
                    ("BOX", (0, 0), (-1, -1), 0.5, border),
                    ("INNERGRID", (0, 0), (-1, -1), 0.35, border),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 7),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 7),
                    ("TOPPADDING", (0, 0), (-1, -1), 5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                ]
            )
        )
        return table

    def callout(content, background_color, border_color):
        table = Table([[content]], colWidths=[164 * mm])
        table.setStyle(
            TableStyle(
                [
                    ("BACKGROUND", (0, 0), (-1, -1), background_color),
                    ("BOX", (0, 0), (-1, -1), 1, border_color),
                    ("LEFTPADDING", (0, 0), (-1, -1), 10),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                    ("TOPPADDING", (0, 0), (-1, -1), 9),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 9),
                ]
            )
        )
        return table

    def draw_page_chrome(canvas, document):
        width, height = A4
        canvas.saveState()
        canvas.setFillColor(red)
        canvas.rect(0, height - 18 * mm, width, 18 * mm, stroke=0, fill=1)
        canvas.setFillColor(colors.white)
        canvas.setFont("Helvetica-Bold", 10.5)
        canvas.drawCentredString(width / 2, height - 11.2 * mm, REPORT_DISCLAIMER)
        canvas.setStrokeColor(border)
        canvas.setLineWidth(0.5)
        canvas.line(23 * mm, 16 * mm, width - 23 * mm, 16 * mm)
        canvas.setFillColor(gray)
        canvas.setFont("Helvetica", 7.5)
        canvas.drawString(23 * mm, 10.5 * mm, REPORT_DISCLAIMER)
        canvas.drawRightString(width - 23 * mm, 10.5 * mm, f"Page {document.page}")
        canvas.setTitle("Rapport technique non certifié diskpurge")
        canvas.setAuthor("diskpurge")
        canvas.setSubject("Trace technique locale d’exécution")
        canvas.restoreState()

    generated_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    source_hash = hashlib.sha256(source).hexdigest()
    document = BaseDocTemplate(
        str(output_path),
        pagesize=A4,
        rightMargin=23 * mm,
        leftMargin=23 * mm,
        topMargin=27 * mm,
        bottomMargin=22 * mm,
        title="Rapport technique non certifié diskpurge",
        author="diskpurge",
        subject="Trace technique locale d’exécution",
        pageCompression=1,
    )
    frame = Frame(
        document.leftMargin,
        document.bottomMargin,
        document.width,
        document.height,
        id="content",
    )
    document.addPageTemplates(
        PageTemplate(id="report", frames=[frame], onPageEnd=draw_page_chrome)
    )
    story = [
        Spacer(1, 3 * mm),
        Paragraph("Rapport technique d’exécution", styles["ReportTitle"]),
        Paragraph(
            "Synthèse lisible d’un journal d’audit diskpurge accepté par le vérificateur local.",
            styles["ReportSubtitle"],
        ),
        callout(
            Paragraph(
                f"{REPORT_DISCLAIMER}<br/><font name='Helvetica' size='9'>Ce document est utilisable uniquement comme trace d’exécution. Il ne constitue ni un certificat d’effacement, ni une preuve de non-récupérabilité, ni une validation indépendante.</font>",
                styles["ReportAlert"],
            ),
            pale_red,
            red,
        ),
        Spacer(1, 7 * mm),
        Paragraph("Identification du rapport", styles["ReportSection"]),
        key_value_table(
            [
                ("Client", client or "non renseigné"),
                ("Référence dossier", reference or "non renseignée"),
                ("Date de génération UTC", generated_at),
                ("Journal source", str(audit_log)),
                ("Empreinte SHA-256 du fichier", source_hash),
                ("Enregistrements acceptés", verification.record_count),
                ("Opérations complètes", len(operations)),
                ("Empreinte finale de chaîne", verification.final_hash),
            ]
        ),
        Spacer(1, 5 * mm),
        Paragraph("Verdict d’usage", styles["ReportSection"]),
        callout(
            Paragraph(
                "Le journal est cohérent avec les contrôles locaux de <b>diskpurge verify-audit</b>. Le présent verdict porte uniquement sur la trace d’exécution analysée ; il ne permet pas de conclure à une certification du support ou de la procédure.",
                styles["ReportBody"],
            ),
            background,
            border,
        ),
        PageBreak(),
        Paragraph("Résultats observés dans le journal", styles["ReportSection"]),
    ]

    summary_rows = [
        [
            paragraph("Opération", "ReportHeaderCell"),
            paragraph("Support", "ReportHeaderCell"),
            paragraph("Méthode", "ReportHeaderCell"),
            paragraph("Statut", "ReportHeaderCell"),
            paragraph("Fin UTC", "ReportHeaderCell"),
        ]
    ]
    for operation in operations:
        end = operation.end
        summary_rows.append(
            [
                paragraph(end["opération"], "ReportSmall"),
                paragraph(end["type_support"], "ReportCell"),
                paragraph(end["méthode_exécutée"], "ReportCell"),
                paragraph(end["statut"].upper(), "ReportKeyCell"),
                paragraph(end["horodatage"], "ReportSmall"),
            ]
        )
    summary_table = LongTable(
        summary_rows,
        colWidths=[48 * mm, 19 * mm, 31 * mm, 25 * mm, 41 * mm],
        repeatRows=1,
        hAlign="LEFT",
    )
    summary_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), navy),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, background]),
                ("BOX", (0, 0), (-1, -1), 0.5, border),
                ("INNERGRID", (0, 0), (-1, -1), 0.35, border),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 5),
                ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]
        )
    )
    story.extend([summary_table, Spacer(1, 5 * mm)])

    for index, operation in enumerate(operations, 1):
        start = operation.start
        end = operation.end
        identity_state = (
            "observée après l’opération"
            if end["identité_après_observée"]
            else "non observée après l’opération"
        )
        native_status = (
            f"{end['statut_natif_brut']} (0x{end['statut_natif_brut']:08x})"
            if end["statut_natif_observé"]
            else "non observé"
        )
        story.extend(
            [
                KeepTogether(
                    [
                        Paragraph(f"Opération {index}", styles["ReportSection"]),
                        callout(
                            Paragraph(
                                f"Résultat déclaré : <b>{html.escape(end['statut'].upper())}</b>. {html.escape(end['détail'])}",
                                styles["ReportBody"],
                            ),
                            pale_blue,
                            navy,
                        ),
                        Spacer(1, 3 * mm),
                    ]
                ),
                key_value_table(
                    [
                        ("Identifiant d’opération", end["opération"]),
                        (
                            "Horodatages UTC",
                            f"début : {start['horodatage']} - fin : {end['horodatage']}",
                        ),
                        (
                            "Identités déclarées",
                            f"opérateur : {format_value(end['opérateur'])} - témoin : {format_value(end['témoin'])}",
                        ),
                        ("Périphérique présenté", end["périphérique"]),
                        ("Identifiant avant", end["identifiant"]),
                        (
                            "Identifiant après",
                            f"{format_value(end['identifiant_après'])} - {identity_state}",
                        ),
                        (
                            "Modèle et firmware",
                            f"{end['modèle']} - firmware : {end['firmware']}",
                        ),
                        ("Transport", end["transport"]),
                        (
                            "Environnement et topologie",
                            f"{end['environnement']} - {end['topologie']}",
                        ),
                        (
                            "Support et capacité",
                            f"{end['type_support']} - {human_size(end['taille'])} - blocs de {end['taille_bloc']} octets",
                        ),
                        (
                            "Méthodes",
                            f"demandée : {end['méthode_demandée']} - exécutée : {end['méthode_exécutée']}",
                        ),
                        ("Sous-méthode native", end["sous_méthode_native"]),
                        ("Vérification consignée", end["vérification"]),
                        (
                            "Statut natif brut et source",
                            f"{native_status} - source : {end['source_statut_natif']}",
                        ),
                        (
                            "Qualification et laboratoire",
                            f"{format_value(end['qualification'])} - laboratoire : {format_value(end['laboratoire'])}",
                        ),
                        ("Version diskpurge", end["version"]),
                        ("Empreinte de l’enregistrement", end["empreinte"]),
                    ]
                ),
                Spacer(1, 5 * mm),
            ]
        )

    limitations = [
        "Aucune validation matérielle n’a été réalisée pour l’émission de ce rapport.",
        "Aucun test indépendant ni audit externe de la procédure n’est joint à ce rapport.",
        "La chaîne SHA-256 du journal est locale, non signée et non ancrée auprès d’un tiers de confiance.",
        "Le vérificateur contrôle la forme, la chaîne et les transitions du journal ; il ne prouve pas l’heure réelle, l’identité des personnes, l’identité physique du support ou l’absence de données résiduelles.",
        "Les identités d’opérateur et de témoin sont des valeurs déclarées dans la source, sans authentification indépendante.",
        "Un statut brut de contrôleur ou de firmware est reproduit comme donnée source et ne vaut pas validation externe du comportement matériel.",
        "Toute modification du PDF, du journal source ou de leur association rompt la portée de cette synthèse.",
        "Le document est utilisable uniquement comme trace d’exécution locale et ne doit pas être intitulé ou présenté comme certificat d’effacement.",
    ]
    limitations_intro = [
        Paragraph("Limites et portée", styles["ReportSection"]),
        callout(
            Paragraph(
                "<b>Ce rapport reste non certifié.</b> Les limites ci-dessous sont constitutives du document et ne doivent pas être retirées lors de sa transmission.",
                styles["ReportBody"],
            ),
            pale_red,
            red,
        ),
        Spacer(1, 4 * mm),
    ]
    story.append(PageBreak())
    story.append(KeepTogether(limitations_intro))
    for limitation in limitations:
        story.append(Paragraph(f"- {html.escape(limitation)}", styles["ReportBody"]))
    story.extend(
        [
            Spacer(1, 5 * mm),
            Paragraph("Contrôle de la source", styles["ReportSection"]),
            key_value_table(
                [
                    (
                        "Arguments du contrôle sans shell",
                        json.dumps(
                            [str(binary), "verify-audit", str(audit_log)],
                            ensure_ascii=False,
                        ),
                    ),
                    (
                        "Sortie du vérificateur",
                        verification.verifier_output.replace("\n", " | "),
                    ),
                    ("Empreinte SHA-256 du journal", source_hash),
                    ("Empreinte finale de chaîne", verification.final_hash),
                ]
            ),
            Spacer(1, 6 * mm),
            callout(
                Paragraph(
                    "CONCLUSION : TRACE D’EXÉCUTION LOCALE COHÉRENTE AVEC LE JOURNAL FOURNI - AUCUNE CERTIFICATION",
                    styles["ReportAlert"],
                ),
                pale_red,
                red,
            ),
        ]
    )
    document.build(story)


def default_output_path(audit_log, repo_root):
    name = re.sub(r"[^A-Za-z0-9._-]+", "-", audit_log.stem).strip("-.") or "journal"
    return repo_root / "output" / "pdf" / f"{name}-rapport-technique.pdf"


def generate_report(
    audit_log, binary, output_path, client="", reference="", overwrite=False
):
    audit_log = Path(audit_log).expanduser().absolute()
    output_path = Path(output_path).expanduser().absolute()
    verification = verify_audit(Path(binary), audit_log)
    source = read_source(audit_log)
    _, operations = parse_jsonl(source, verification)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists() and not overwrite:
        raise ReportError(
            f"La sortie existe déjà : {output_path}. Utilisez --force pour la remplacer."
        )
    try:
        if output_path.exists() and os.path.samefile(audit_log, output_path):
            raise ReportError(
                "Le rapport de sortie ne peut pas remplacer le journal source."
            )
    except FileNotFoundError:
        pass
    file_descriptor, temporary_path = tempfile.mkstemp(
        prefix=f".{output_path.stem}-", suffix=".pdf", dir=str(output_path.parent)
    )
    os.close(file_descriptor)
    try:
        create_pdf(
            Path(temporary_path),
            audit_log,
            binary,
            source,
            verification,
            operations,
            client,
            reference,
        )
        if Path(temporary_path).stat().st_size < 1024:
            raise ReportError("Le PDF généré est anormalement petit.")
        os.replace(temporary_path, output_path)
    except Exception:
        try:
            os.unlink(temporary_path)
        except FileNotFoundError:
            pass
        raise
    return output_path


def build_parser():
    parser = argparse.ArgumentParser(
        description="Génère un rapport PDF technique non certifié depuis un journal diskpurge vérifié."
    )
    parser.add_argument(
        "audit_log", metavar="journal", help="journal d’audit JSONL à analyser"
    )
    parser.add_argument(
        "-o", "--output", dest="output_path", help="chemin du PDF de sortie"
    )
    parser.add_argument(
        "--diskpurge", dest="binary", help="chemin du binaire diskpurge"
    )
    parser.add_argument("--client", default="", help="nom ou référence du client")
    parser.add_argument(
        "--reference",
        dest="reference",
        default="",
        help="référence interne du dossier",
    )
    parser.add_argument(
        "--force",
        dest="overwrite",
        action="store_true",
        help="remplace un PDF existant",
    )
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_root = Path(__file__).resolve().parents[2]
    audit_log = Path(args.audit_log).expanduser().absolute()
    output_path = (
        Path(args.output_path).expanduser().absolute()
        if args.output_path
        else default_output_path(audit_log, repo_root)
    )
    try:
        binary = find_binary(args.binary, repo_root)
        result = generate_report(
            audit_log=audit_log,
            binary=binary,
            output_path=output_path,
            client=args.client,
            reference=args.reference,
            overwrite=args.overwrite,
        )
    except ReportError as error:
        print(f"Erreur : {error}", file=sys.stderr)
        return 2
    except Exception as error:
        print(f"Erreur inattendue : {error}", file=sys.stderr)
        return 3
    print(f"Rapport créé : {result}")
    print(REPORT_DISCLAIMER)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
