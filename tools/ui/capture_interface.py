#!/usr/bin/env python3

import argparse
import os
import sys
from importlib import import_module
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QFontDatabase, QImage
from PySide6.QtWidgets import QApplication, QMessageBox

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
os.environ.setdefault("QT_SCALE_FACTOR", "1")
os.environ.setdefault("QT_AUTO_SCREEN_SCALE_FACTOR", "0")

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

DatafuckerrWindow = import_module("ui.datafuckerr_qt").DatafuckerrWindow


DEMO_IDENTIFIER = "52266dc2ff8067e68104607e750abb9d3b36582b8af909fcb58"
DEMO_FINGERPRINT = "34de4978a65284e25fd1bad1942892e9f003c373dc9813803076bbf6db0bd38e"


def disable_commands(window):
    def reject(*args, **kwargs):
        raise RuntimeError("La capture simulée interdit tout lancement de commande.")

    window.run_command = reject


def set_reset_state(window):
    window.navigate("erase")
    window.invalidate_preparation()
    window.device_selector.blockSignals(True)
    window.device_selector.clear()
    window.device_selector.addItem("Aucun support recherché", "")
    window.device_selector.setCurrentIndex(0)
    window.device_selector.blockSignals(False)
    window.erase_target.clear()
    window.prepare_button.setEnabled(False)


def set_confirmation_state(window):
    window.navigate("erase")
    window.erase_target.blockSignals(True)
    window.erase_target.setText("/simulation/support-demo.img")
    window.erase_target.blockSignals(False)
    window.preparation_snapshot = {
        "binary": "/simulation/diskpurge",
        "target": "/simulation/support-demo.img",
        "method": window.erase_method.currentText(),
        "verification": window.erase_verification.currentText(),
        "inspection": {"Identifiant": DEMO_IDENTIFIER},
    }
    window.confirmed_target.setText("/simulation/support-demo.img")
    window.observed_identifier.setText(DEMO_IDENTIFIER)
    window.entered_identifier.clear()
    window.entered_identifier.setPlaceholderText(f"Recopiez : {DEMO_IDENTIFIER[-6:]}")
    window.erase_log.setText("/simulation/datafuckerr/ACME-DEMO-071.jsonl")
    window.automatic_log = True
    window.report_destination.setText(
        "Enregistrement automatique dans le dossier datafuckerr"
    )
    window.operator.setText("OPÉRATEUR-DÉMO")
    window.witness.setText("TÉMOIN-DÉMO")
    window.acknowledgment.setChecked(False)
    window.preparation_pill.set_content("PLAN VALIDÉ", "success")
    window.erase_status.set_content("CONFIRMATION REQUISE", "warning")
    window.erase_detail.setText(
        "État de démonstration : aucune commande ne sera exécutée."
    )
    window._update_erase_authorization()
    window.selection_section.hide()
    window.section_confirmation.show()
    window._activate_step(2)
    window.entered_identifier.setFocus()


def set_reports_state(window):
    window.navigate("reports")
    window.audit_path.setText("/simulation/datafuckerr/ACME-DEMO-071.jsonl")
    window.audit_pill.set_content("JOURNAL VALIDE", "success")
    window.audit_fingerprint.setText(DEMO_FINGERPRINT)
    window.report_button.setEnabled(True)
    window.audit_path.setFocus()


def set_dialog_state(window):
    return window._create_final_confirmation(
        "/simulation/support-demo.img",
        DEMO_IDENTIFIER,
    )


STATES = {
    "reset": set_reset_state,
    "confirmation": set_confirmation_state,
    "reports": set_reports_state,
    "dialog": set_dialog_state,
}

FILES = {
    "reset": "datafuckerr-reinitialisation-simple-2k.png",
    "confirmation": "datafuckerr-confirmation-simple-2k.png",
    "reports": "datafuckerr-rapports-simple-2k.png",
    "dialog": "datafuckerr-dialogue-final.png",
}


def settle_ui(application, window):
    for unused in range(8):
        application.processEvents()
        window.repaint()


def capture_window(application, window, destination, width, height):
    window.hide()
    application.processEvents()
    window.resize(width, height)
    window.show()
    settle_ui(application, window)
    image = QImage(width, height, QImage.Format.Format_RGB888)
    image.fill(Qt.GlobalColor.white)
    window.render(image)
    image.setDotsPerMeterX(3780)
    image.setDotsPerMeterY(3780)
    if image.width() != width or image.height() != height:
        raise RuntimeError(
            f"Dimensions inattendues : {image.width()}×{image.height()}, attendu {width}×{height}."
        )
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not image.save(str(destination), "PNG"):
        raise RuntimeError(f"Impossible d’enregistrer {destination}.")


def capture_dialog(application, dialog, destination):
    dialog.adjustSize()
    dialog.show()
    settle_ui(application, dialog)
    image = QImage(dialog.width(), dialog.height(), QImage.Format.Format_RGB888)
    image.fill(Qt.GlobalColor.white)
    dialog.render(image)
    image.setDotsPerMeterX(3780)
    image.setDotsPerMeterY(3780)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not image.save(str(destination), "PNG"):
        raise RuntimeError(f"Impossible d’enregistrer {destination}.")
    dialog.close()


def build_parser():
    parser = argparse.ArgumentParser(
        description="Capture hors écran de quatre états datafuckerr entièrement simulés."
    )
    parser.add_argument(
        "--state",
        choices=("all", *STATES),
        default="all",
        help="état unique à capturer, ou tous les états",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "output" / "screenshots",
        help="dossier de destination des images PNG",
    )
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.width < 1180 or args.height < 760:
        raise SystemExit(
            "La capture doit respecter la taille minimale de l’interface : 1180×760."
        )
    QApplication.setAttribute(Qt.ApplicationAttribute.AA_Use96Dpi, True)
    application = QApplication.instance() or QApplication([])
    application.setApplicationName("datafuckerr — Capture simulée")
    application.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.GeneralFont))
    state_names = tuple(STATES) if args.state == "all" else (args.state,)
    window = DatafuckerrWindow("/simulation/diskpurge-inexistant")
    disable_commands(window)
    window.resize(args.width, args.height)
    window.show()
    settle_ui(application, window)
    for name in state_names:
        destination = args.output.expanduser().resolve() / FILES[name]
        result = STATES[name](window)
        if isinstance(result, QMessageBox):
            capture_dialog(application, result, destination)
        else:
            window.centralWidget().updateGeometry()
            window.centralWidget().update()
            capture_window(application, window, destination, args.width, args.height)
        print(destination)
    window.close()
    application.processEvents()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
