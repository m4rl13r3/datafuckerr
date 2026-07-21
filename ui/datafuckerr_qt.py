#!/usr/bin/env python3

import argparse
import sys
from datetime import datetime
from pathlib import Path

from PySide6.QtCore import QProcess, Qt
from PySide6.QtGui import QFontDatabase, QKeySequence, QShortcut, QTextCursor
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

try:
    from .diskpurge_commands import (
        METHODS,
        VERIFICATIONS,
        assert_safe_command,
        build_erase_command,
        build_inspect_command,
        build_list_command,
        build_plan_command,
        build_verify_audit_command,
        build_version_command,
        default_binary_path,
        displayed_command,
        inspection_signature,
        parse_inspection,
        plan_is_executable,
        validated_choice,
        validated_text,
    )
except ImportError:
    from diskpurge_commands import (
        METHODS,
        VERIFICATIONS,
        assert_safe_command,
        build_erase_command,
        build_inspect_command,
        build_list_command,
        build_plan_command,
        build_verify_audit_command,
        build_version_command,
        default_binary_path,
        displayed_command,
        inspection_signature,
        parse_inspection,
        plan_is_executable,
        validated_choice,
        validated_text,
    )


def configure_accessibility(widget, name, description=""):
    widget.setAccessibleName(name)
    if description:
        widget.setAccessibleDescription(description)
    return widget


def refresh_style(widget):
    widget.style().unpolish(widget)
    widget.style().polish(widget)
    widget.update()


def make_label(text, role="body", wrap=False):
    label = QLabel(text)
    label.setProperty("role", role)
    label.setWordWrap(wrap)
    return label


class Card(QFrame):
    def __init__(self, parent=None, compact=False):
        super().__init__(parent)
        self.setProperty("role", "cardCompact" if compact else "card")


class Pill(QLabel):
    def __init__(self, text, tone="neutral", parent=None):
        super().__init__(text, parent)
        self.setProperty("role", "pill")
        self.setProperty("tone", tone)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        configure_accessibility(self, text)

    def set_content(self, text, tone="neutral"):
        self.setText(text)
        self.setAccessibleName(text)
        self.setProperty("tone", tone)
        refresh_style(self)


class DatafuckerrWindow(QMainWindow):
    def __init__(self, binary_path=None):
        super().__init__()
        self.binary_path = str(Path(binary_path or default_binary_path()).expanduser())
        self.process = None
        self.process_output = []
        self.process_callback = None
        self.process_console = None
        self.command_buttons = []
        self.preparation_snapshot = None
        self.pending_inspection = None
        self.high_contrast = False
        self.scale = 1.0
        self.setWindowTitle("datafuckerr — Centre de sanitisation")
        self.setMinimumSize(1180, 760)
        self.resize(1520, 940)
        self._build_ui()
        self._install_shortcuts()
        self._apply_theme()
        self.navigate("erase")

    def _build_ui(self):
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._build_navigation_bar())
        layout.addWidget(self._build_content(), 1)

    def _build_navigation_bar(self):
        navigation_bar = QFrame()
        navigation_bar.setObjectName("MainNavigation")
        navigation_bar.setFixedHeight(64)
        layout = QHBoxLayout(navigation_bar)
        layout.setContentsMargins(24, 8, 24, 8)
        layout.setSpacing(8)

        brand_layout = QHBoxLayout()
        brand_layout.setSpacing(11)
        logo = QLabel("DF")
        logo.setObjectName("Logo")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setFixedSize(36, 36)
        brand_layout.addWidget(logo)
        identity = QVBoxLayout()
        identity.setSpacing(1)
        identity.addWidget(make_label("datafuckerr", "brand"))
        identity.addWidget(make_label("Réinitialisation sécurisée", "brandSub"))
        brand_layout.addLayout(identity)
        layout.addLayout(brand_layout)
        layout.addSpacing(24)

        self.navigation_buttons = {}
        items = (
            ("erase", "Réinitialiser", "Alt+1"),
            ("reports", "Rapports", "Alt+2"),
        )
        for identifier, label, shortcut in items:
            button = QPushButton(label)
            button.setCheckable(True)
            button.setProperty("role", "nav")
            button.setMinimumSize(120, 36)
            button.clicked.connect(
                lambda checked=False, page=identifier: self.navigate(page)
            )
            configure_accessibility(
                button, label, f"Ouvre {label}. Raccourci {shortcut}."
            )
            layout.addWidget(button)
            self.navigation_buttons[identifier] = button

        layout.addStretch()
        self.engine_pill = Pill("STANDARD", "success")
        layout.addWidget(self.engine_pill)
        self.engine_pill.hide()
        self.binary_label = make_label(Path(self.binary_path).name, "bodyStrong")
        self.binary_label.hide()
        self.binary_detail = make_label(self.binary_path, "micro", True)
        self.binary_detail.hide()
        settings_button = QPushButton("Réglages")
        settings_button.setProperty("role", "ghost")
        settings_button.clicked.connect(self.select_binary)
        configure_accessibility(settings_button, "Changer le binaire diskpurge")
        layout.addWidget(settings_button)
        return navigation_bar

    def _build_content(self):
        container = QFrame()
        container.setObjectName("Content")
        layout = QVBoxLayout(container)
        layout.setContentsMargins(32, 24, 32, 24)
        layout.setSpacing(0)

        self.breadcrumb = make_label("datafuckerr  /  ACCUEIL", "breadcrumb")
        self.breadcrumb.setParent(container)
        self.breadcrumb.hide()
        self.contrast_button = QPushButton(container)
        self.contrast_button.setCheckable(True)
        self.contrast_button.hide()

        self.pages = QStackedWidget()
        self.pages.setObjectName("Pages")
        self.page_indexes = {}
        for name, builder in (
            ("erase", self._build_reset_page),
            ("reports", self._build_reports_page),
        ):
            self.page_indexes[name] = self.pages.addWidget(builder())
        layout.addWidget(self.pages, 1)
        return container

    def _build_page_header(self, eyebrow, title, description, action=None):
        container = QWidget()
        outer_layout = QHBoxLayout(container)
        outer_layout.setContentsMargins(0, 0, 0, 0)
        content = QWidget()
        content.setMaximumWidth(1040)
        layout = QHBoxLayout(content)
        layout.setContentsMargins(0, 4, 0, 24)
        text_layout = QVBoxLayout()
        text_layout.setSpacing(6)
        text_layout.addWidget(make_label(eyebrow.upper(), "eyebrowAccent"))
        text_layout.addWidget(make_label(title, "pageTitle"))
        text_layout.addWidget(make_label(description, "pageLead", True))
        layout.addLayout(text_layout, 1)
        if action is not None:
            layout.addWidget(action, 0, Qt.AlignmentFlag.AlignBottom)
        outer_layout.addStretch()
        outer_layout.addWidget(content, 1)
        outer_layout.addStretch()
        return container

    def _build_step(self, number, title, detail, active=False):
        card = QFrame()
        card.setProperty("role", "step")
        card.setProperty("active", active)
        card.setMinimumWidth(200)
        card.setSizePolicy(QSizePolicy.Policy.Maximum, QSizePolicy.Policy.Fixed)
        layout = QHBoxLayout(card)
        layout.setContentsMargins(0, 8, 0, 8)
        layout.setSpacing(10)
        number_label = QLabel(str(number))
        number_label.setProperty("role", "stepNumber")
        number_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        number_label.setFixedSize(28, 28)
        layout.addWidget(number_label)
        text_layout = QVBoxLayout()
        text_layout.setSpacing(1)
        text_layout.addWidget(make_label(title, "bodyStrong"))
        text_layout.addWidget(make_label(detail, "micro"))
        layout.addLayout(text_layout, 1)
        configure_accessibility(card, f"Étape {number}, {title}. {detail}")
        return card

    def _build_reset_page(self):
        page = QWidget()
        page_layout = QVBoxLayout(page)
        page_layout.setContentsMargins(0, 0, 0, 0)
        page_layout.setSpacing(12)
        page_layout.addWidget(
            self._build_page_header(
                "Réinitialisation",
                "Réinitialiser un support",
                "Analysez le support, vérifiez son identité, puis confirmez la suppression définitive.",
            )
        )

        steps_container = QWidget()
        steps_container.setMaximumWidth(1040)
        steps_layout = QHBoxLayout(steps_container)
        steps_layout.setContentsMargins(0, 0, 0, 0)
        steps_layout.setSpacing(12)
        self.target_step = self._build_step(1, "Analyser", "Choisir le support", True)
        self.identity_step = self._build_step(2, "Confirmer", "Vérifier l’identité")
        self.execution_step = self._build_step(
            3, "Réinitialiser", "Supprimer les données"
        )
        for index, step in enumerate(
            (self.target_step, self.identity_step, self.execution_step)
        ):
            steps_layout.addWidget(step)
            if index < 2:
                separator = QFrame()
                separator.setProperty("role", "stepSeparator")
                steps_layout.addWidget(separator, 1)
        steps_row = QHBoxLayout()
        steps_row.addStretch()
        steps_row.addWidget(steps_container, 1)
        steps_row.addStretch()
        page_layout.addLayout(steps_row)

        scroll_area = QScrollArea()
        scroll_area.setObjectName("EffacementScroll")
        scroll_area.setWidgetResizable(True)
        scroll_area.setFrameShape(QFrame.Shape.NoFrame)
        scroll_area.viewport().setObjectName("EffacementViewport")
        content = QWidget()
        content.setObjectName("EffacementContenu")
        layout = QVBoxLayout(content)
        layout.setContentsMargins(0, 8, 0, 8)
        layout.setSpacing(12)

        selection_card = Card()
        self.selection_section = selection_card
        selection_card.setMaximumWidth(1040)
        selection_layout = QVBoxLayout(selection_card)
        selection_layout.setContentsMargins(24, 24, 24, 24)
        selection_layout.setSpacing(12)
        selection_layout.addWidget(
            make_label("Quel support voulez-vous réinitialiser ?", "sectionTitle")
        )
        selection_layout.addWidget(
            make_label(
                "Recherchez les supports connectés, puis choisissez celui à analyser.",
                "muted",
                True,
            )
        )
        device_row = QHBoxLayout()
        self.device_selector = configure_accessibility(
            QComboBox(), "Support à réinitialiser"
        )
        self.device_selector.addItem("Aucun support recherché", "")
        self.detect_button = QPushButton("Rechercher les supports")
        self.detect_button.clicked.connect(self.search_devices)
        configure_accessibility(
            self.detect_button, "Rechercher les supports connectés sans les modifier"
        )
        self.command_buttons.append(self.detect_button)
        device_row.addWidget(self.device_selector, 1)
        device_row.addWidget(self.detect_button)
        selection_layout.addLayout(device_row)
        self.erase_target = configure_accessibility(
            QLineEdit(),
            "Chemin du support à réinitialiser",
            "La valeur doit être saisie explicitement",
        )
        self.erase_target.setPlaceholderText("Chemin manuel, par exemple /dev/disk4")
        self.prepare_button = QPushButton("Analyser ce support")
        self.prepare_button.setProperty("role", "primary")
        self.prepare_button.setEnabled(False)
        self.prepare_button.clicked.connect(self.prepare_erase)
        configure_accessibility(
            self.prepare_button, "Analyser le support sans modifier les données"
        )
        self.command_buttons.append(self.prepare_button)
        self.options_button = QPushButton("Options avancées")
        self.options_button.setCheckable(True)
        self.options_button.setProperty("role", "ghost")
        configure_accessibility(
            self.options_button, "Afficher les options avancées d’effacement"
        )
        selection_footer = QHBoxLayout()
        selection_footer.addWidget(self.options_button)
        selection_footer.addStretch()
        self.prepare_button.setMinimumWidth(180)
        selection_footer.addWidget(self.prepare_button)
        selection_layout.addLayout(selection_footer)
        self.options_area = QWidget()
        options_layout = QGridLayout(self.options_area)
        options_layout.setContentsMargins(0, 4, 0, 0)
        options_layout.setHorizontalSpacing(12)
        self.erase_method = configure_accessibility(QComboBox(), "Méthode d’effacement")
        self.erase_method.addItems(METHODS)
        self.erase_verification = configure_accessibility(
            QComboBox(), "Niveau de vérification après effacement"
        )
        self.erase_verification.addItems(VERIFICATIONS)
        options_layout.addWidget(make_label("Chemin manuel", "fieldLabel"), 0, 0, 1, 2)
        options_layout.addWidget(self.erase_target, 1, 0, 1, 2)
        options_layout.addWidget(make_label("Méthode", "fieldLabel"), 2, 0)
        options_layout.addWidget(make_label("Vérification", "fieldLabel"), 2, 1)
        options_layout.addWidget(self.erase_method, 3, 0)
        options_layout.addWidget(self.erase_verification, 3, 1)
        options_layout.setColumnStretch(0, 1)
        options_layout.setColumnStretch(1, 1)
        self.options_area.hide()
        self.options_button.toggled.connect(self.options_area.setVisible)
        selection_layout.addWidget(self.options_area)

        self.status_area = QFrame()
        self.status_area.setProperty("role", "status")
        status_layout = QHBoxLayout(self.status_area)
        status_layout.setContentsMargins(12, 10, 12, 10)
        self.erase_status = Pill("PRÊT À ANALYSER", "neutral")
        status_layout.addWidget(self.erase_status)
        self.erase_detail = make_label("L’analyse est non destructive.", "muted", True)
        status_layout.addWidget(self.erase_detail, 1)
        self.status_area.hide()
        selection_layout.addWidget(self.status_area)

        selection_row = QHBoxLayout()
        selection_row.addStretch()
        selection_row.addWidget(selection_card, 1)
        selection_row.addStretch()
        layout.addLayout(selection_row)

        self.section_confirmation = Card()
        self.section_confirmation.setMaximumWidth(1040)
        confirmation_layout = QGridLayout(self.section_confirmation)
        confirmation_layout.setContentsMargins(24, 24, 24, 24)
        confirmation_layout.setHorizontalSpacing(12)
        confirmation_layout.setVerticalSpacing(12)
        confirmation_header = QHBoxLayout()
        confirmation_header.addWidget(
            make_label("Vérifiez avant de continuer", "sectionTitle")
        )
        confirmation_header.addStretch()
        change_device_button = QPushButton("Changer de support")
        change_device_button.setProperty("role", "link")
        change_device_button.clicked.connect(self.invalidate_preparation)
        confirmation_header.addWidget(change_device_button)
        self.preparation_pill = Pill("À ANALYSER", "neutral")
        confirmation_header.addWidget(self.preparation_pill)
        confirmation_layout.addLayout(confirmation_header, 0, 0, 1, 4)
        confirmation_layout.addWidget(
            make_label("Cible à effacer", "fieldLabel"), 1, 0, 1, 4
        )
        self.confirmed_target = configure_accessibility(
            QLineEdit("—"), "Cible à effacer confirmée"
        )
        self.confirmed_target.setReadOnly(True)
        confirmation_layout.addWidget(self.confirmed_target, 2, 0, 1, 4)
        confirmation_layout.addWidget(
            make_label("Identifiant observé", "fieldLabel"), 3, 0, 1, 2
        )
        confirmation_layout.addWidget(
            make_label("Recopiez les 6 derniers caractères", "fieldLabel"), 3, 2, 1, 2
        )
        self.observed_identifier = configure_accessibility(
            QLineEdit("—"), "Identifiant stable observé"
        )
        self.observed_identifier.setReadOnly(True)
        self.entered_identifier = configure_accessibility(
            QLineEdit(), "Identifiant stable recopié"
        )
        self.entered_identifier.setPlaceholderText("6 caractères")
        confirmation_layout.addWidget(self.observed_identifier, 4, 0, 1, 2)
        confirmation_layout.addWidget(self.entered_identifier, 4, 2, 1, 2)
        self.erase_log = configure_accessibility(
            QLineEdit(), "Chemin du journal d’audit distinct"
        )
        self.erase_log.hide()
        self.automatic_log = True
        self.report_destination = make_label(
            "Enregistrement automatique dans le dossier datafuckerr", "muted", True
        )
        select_log_button = QPushButton("Modifier…")
        select_log_button.clicked.connect(self.select_erase_log)
        confirmation_layout.addWidget(
            make_label("Rapport technique", "fieldLabel"), 5, 0, 1, 4
        )
        confirmation_layout.addWidget(self.report_destination, 6, 0, 1, 3)
        confirmation_layout.addWidget(select_log_button, 6, 3)
        self.operator = configure_accessibility(
            QLineEdit(), "Identifiant de l’opérateur"
        )
        self.operator.setPlaceholderText("Opérateur")
        self.witness = configure_accessibility(
            QLineEdit(), "Identifiant du témoin distinct"
        )
        self.witness.setPlaceholderText("Témoin")
        confirmation_layout.addWidget(make_label("Opérateur", "fieldLabel"), 7, 0, 1, 2)
        confirmation_layout.addWidget(
            make_label("Témoin distinct", "fieldLabel"), 7, 2, 1, 2
        )
        confirmation_layout.addWidget(self.operator, 8, 0, 1, 2)
        confirmation_layout.addWidget(self.witness, 8, 2, 1, 2)
        self.acknowledgment = QCheckBox(
            "Je confirme la suppression définitive de toutes les données de ce support."
        )
        configure_accessibility(
            self.acknowledgment,
            "Confirmation explicite de la suppression définitive des données",
        )
        confirmation_layout.addWidget(self.acknowledgment, 9, 0, 1, 4)
        self.erase_button = QPushButton("Réinitialiser définitivement")
        self.erase_button.setProperty("role", "danger")
        self.erase_button.setEnabled(False)
        self.erase_button.clicked.connect(self.execute_erase)
        configure_accessibility(
            self.erase_button, "Réinspecter puis lancer l’effacement irréversible"
        )
        self.command_buttons.append(self.erase_button)
        confirmation_actions = QHBoxLayout()
        confirmation_actions.addStretch()
        self.erase_button.setMinimumWidth(240)
        confirmation_actions.addWidget(self.erase_button)
        confirmation_layout.addLayout(confirmation_actions, 10, 0, 1, 4)
        self.section_confirmation.hide()
        confirmation_row = QHBoxLayout()
        confirmation_row.addStretch()
        confirmation_row.addWidget(self.section_confirmation, 1)
        confirmation_row.addStretch()
        layout.addLayout(confirmation_row)

        self.details_button = QPushButton("Afficher les détails techniques")
        self.details_button.setCheckable(True)
        self.details_button.setProperty("role", "link")
        self.details_button.hide()
        self.options_button.toggled.connect(self.details_button.setVisible)
        layout.addWidget(self.details_button, 0, Qt.AlignmentFlag.AlignCenter)
        self.technical_area = Card()
        self.technical_area.setMaximumWidth(1040)
        technical_layout = QVBoxLayout(self.technical_area)
        technical_layout.setContentsMargins(18, 16, 18, 16)
        technical_layout.addWidget(make_label("Détails techniques", "sectionTitle"))
        self.erase_console = self._create_console(
            "Journal technique de la réinitialisation"
        )
        self.erase_console.setMinimumHeight(160)
        technical_layout.addWidget(self.erase_console)
        self.technical_area.hide()
        self.details_button.toggled.connect(self.technical_area.setVisible)
        self.options_button.toggled.connect(
            lambda visible: (
                self.details_button.setChecked(False) if not visible else None
            )
        )
        technical_row = QHBoxLayout()
        technical_row.addStretch()
        technical_row.addWidget(self.technical_area, 1)
        technical_row.addStretch()
        layout.addLayout(technical_row)
        layout.addStretch()
        scroll_area.setWidget(content)
        page_layout.addWidget(scroll_area, 1)

        self.erase_target.textChanged.connect(self.invalidate_preparation)
        self.device_selector.currentIndexChanged.connect(self.select_device)
        self.erase_method.currentTextChanged.connect(self.invalidate_preparation)
        self.erase_verification.currentTextChanged.connect(self.invalidate_preparation)
        self.entered_identifier.textChanged.connect(self._update_erase_authorization)
        self.erase_log.textChanged.connect(self._update_erase_authorization)
        self.operator.textChanged.connect(self._update_erase_authorization)
        self.witness.textChanged.connect(self._update_erase_authorization)
        self.acknowledgment.toggled.connect(self._update_erase_authorization)
        return page

    def _build_reports_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        layout.addWidget(
            self._build_page_header(
                "Rapports",
                "Vérifier un rapport technique",
                "Choisissez un journal datafuckerr pour contrôler son intégrité ou créer un PDF.",
            )
        )

        card = Card()
        card.setMaximumWidth(1040)
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(24, 24, 24, 24)
        card_layout.setSpacing(12)
        card_layout.addWidget(make_label("Journal à vérifier", "sectionTitle"))
        file_row = QHBoxLayout()
        self.audit_path = configure_accessibility(
            QLineEdit(), "Chemin du journal datafuckerr à vérifier"
        )
        self.audit_path.setPlaceholderText("Choisissez un fichier .jsonl")
        select_button = QPushButton("Choisir…")
        select_button.clicked.connect(self.select_audit)
        file_row.addWidget(self.audit_path, 1)
        file_row.addWidget(select_button)
        card_layout.addLayout(file_row)
        verify_button = QPushButton("Vérifier le journal")
        verify_button.setProperty("role", "primary")
        verify_button.clicked.connect(self.verify_audit)
        self.command_buttons.append(verify_button)
        verify_row = QHBoxLayout()
        verify_row.addStretch()
        verify_button.setMinimumWidth(180)
        verify_row.addWidget(verify_button)
        card_layout.addLayout(verify_row)

        result = QFrame()
        result.setProperty("role", "status")
        result_layout = QVBoxLayout(result)
        result_layout.setContentsMargins(14, 12, 14, 12)
        status_row = QHBoxLayout()
        status_row.addWidget(make_label("Résultat", "bodyStrong"))
        status_row.addStretch()
        self.audit_pill = Pill("NON VÉRIFIÉ", "neutral")
        status_row.addWidget(self.audit_pill)
        result_layout.addLayout(status_row)
        self.audit_fingerprint = make_label(
            "Aucune empreinte disponible.", "mono", True
        )
        result_layout.addWidget(self.audit_fingerprint)
        card_layout.addWidget(result)

        self.report_button = QPushButton("Créer le PDF")
        self.report_button.setEnabled(False)
        self.report_button.clicked.connect(self.generate_report)
        self.command_buttons.append(self.report_button)
        report_row = QHBoxLayout()
        report_row.addStretch()
        self.report_button.setMinimumWidth(180)
        report_row.addWidget(self.report_button)
        card_layout.addLayout(report_row)
        card_row = QHBoxLayout()
        card_row.addStretch()
        card_row.addWidget(card, 1)
        card_row.addStretch()
        layout.addLayout(card_row)

        details = QPushButton("Afficher les détails techniques")
        details.setCheckable(True)
        details.setProperty("role", "link")
        layout.addWidget(details, 0, Qt.AlignmentFlag.AlignCenter)
        console_area = Card()
        console_area.setMaximumWidth(1040)
        console_layout = QVBoxLayout(console_area)
        console_layout.setContentsMargins(18, 16, 18, 16)
        self.audit_console = self._create_console(
            "Détails de la vérification du journal"
        )
        self.audit_console.setMinimumHeight(180)
        console_layout.addWidget(self.audit_console)
        console_area.hide()
        details.toggled.connect(console_area.setVisible)
        console_row = QHBoxLayout()
        console_row.addStretch()
        console_row.addWidget(console_area, 1)
        console_row.addStretch()
        layout.addLayout(console_row)
        layout.addStretch()
        return page

    def _create_console(self, name):
        console = QPlainTextEdit()
        console.setReadOnly(True)
        console.setLineWrapMode(QPlainTextEdit.LineWrapMode.WidgetWidth)
        console.setPlaceholderText("Les messages du moteur apparaîtront ici.")
        configure_accessibility(console, name)
        return console

    def _install_shortcuts(self):
        self.shortcuts = []
        for sequence, page in (("Alt+1", "erase"), ("Alt+2", "reports")):
            shortcut = QShortcut(QKeySequence(sequence), self)
            shortcut.activated.connect(
                lambda destination=page: self.navigate(destination)
            )
            self.shortcuts.append(shortcut)
        for sequence, action in (
            ("Ctrl++", lambda: self.adjust_scale(0.1)),
            ("Ctrl+-", lambda: self.adjust_scale(-0.1)),
            ("Ctrl+0", self.reset_scale),
            ("Ctrl+Shift+C", self.toggle_contrast),
        ):
            shortcut = QShortcut(QKeySequence(sequence), self)
            shortcut.activated.connect(action)
            self.shortcuts.append(shortcut)

    def navigate(self, page):
        if page not in self.page_indexes:
            return
        self.pages.setCurrentIndex(self.page_indexes[page])
        titles = {
            "home": "ACCUEIL",
            "devices": "SUPPORTS",
            "erase": "EFFACEMENT GUIDÉ",
            "reports": "JOURNAUX & RAPPORTS",
        }
        self.breadcrumb.setText(f"datafuckerr  /  {titles[page]}")
        for name, button in self.navigation_buttons.items():
            button.setChecked(name == page)

    def select_binary(self):
        path, unused = QFileDialog.getOpenFileName(
            self, "Choisir le binaire diskpurge", str(Path(self.binary_path).parent)
        )
        if not path:
            return
        self.binary_path = path
        self.binary_label.setText(Path(path).name)
        self.binary_detail.setText(path)
        self.invalidate_preparation()

    def _resolve_binary(self):
        value = validated_text(self.binary_path, "Le binaire")
        path = Path(value).expanduser().resolve()
        if not path.is_file():
            raise ValueError(f"Le binaire diskpurge est introuvable : {path}")
        return str(path)

    def _show_error(self, message):
        QMessageBox.critical(self, "datafuckerr", str(message))

    def _set_busy(self, busy):
        for button in self.command_buttons:
            button.setEnabled(not busy)
        if not busy:
            self.prepare_button.setEnabled(bool(self.erase_target.text().strip()))
            self._update_erase_authorization()
            self.report_button.setEnabled(self.audit_pill.text() == "JOURNAL VALIDE")

    def _append_console(self, console, text, clear_output=False):
        if console is None:
            return
        if clear_output:
            console.clear()
        console.moveCursor(QTextCursor.MoveOperation.End)
        console.insertPlainText(text)
        console.ensureCursorVisible()

    def run_command(self, command, console, callback=None, clear_output=True):
        if self.process is not None:
            self._show_error("Une commande est déjà en cours.")
            return
        try:
            assert_safe_command(command)
        except ValueError as error:
            self._show_error(error)
            return
        self.process_output = []
        self.process_callback = callback
        self.process_console = console
        self._append_console(
            console, f"$ {displayed_command(command)}\n\n", clear_output
        )
        self._set_busy(True)
        process = QProcess(self)
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.setProgram(command[0])
        process.setArguments(command[1:])
        process.readyReadStandardOutput.connect(self._read_process_output)
        process.finished.connect(self._finish_process)
        process.errorOccurred.connect(self._handle_process_error)
        self.process = process
        process.start()

    def _read_process_output(self):
        if self.process is None:
            return
        text = bytes(self.process.readAllStandardOutput()).decode(
            "utf-8", errors="replace"
        )
        if text:
            self.process_output.append(text)
            self._append_console(self.process_console, text)

    def _handle_process_error(self, error):
        if self.process is None:
            return
        if error == QProcess.ProcessError.FailedToStart:
            message = f"Impossible de lancer la commande : {self.process.errorString()}"
            self._append_console(self.process_console, f"\n{message}\n")
            output = "".join(self.process_output)
            callback = self.process_callback
            self.process.deleteLater()
            self.process = None
            self.process_callback = None
            self.process_console = None
            self._set_busy(False)
            if callback is not None:
                callback(-1, output, message)

    def _finish_process(self, code, state):
        self._read_process_output()
        output = "".join(self.process_output)
        error = None
        if state == QProcess.ExitStatus.CrashExit:
            error = "Le processus s’est interrompu anormalement."
        self._append_console(self.process_console, f"\nCode de sortie : {code}\n")
        callback = self.process_callback
        self.process.deleteLater()
        self.process = None
        self.process_callback = None
        self.process_console = None
        self._set_busy(False)
        if callback is not None:
            callback(code, output, error)

    def search_devices(self):
        try:
            command = build_list_command(self._resolve_binary())
        except ValueError as error:
            self._show_error(error)
            return
        self.status_area.show()
        self.erase_status.set_content("RECHERCHE EN COURS", "blue")
        self.erase_detail.setText("Inventaire non destructif des supports connectés.")
        self.run_command(command, self.erase_console, self._after_device_search)

    def _after_device_search(self, code, output, error):
        self.device_selector.blockSignals(True)
        self.device_selector.clear()
        devices = []
        if not error and code == 0:
            for row in output.splitlines():
                columns = row.split("\t")
                if columns and columns[0].strip():
                    path = columns[0].strip()
                    details = " · ".join(
                        value.strip() for value in columns[1:] if value.strip()
                    )
                    devices.append((path, f"{path} — {details}" if details else path))
        if devices:
            self.device_selector.addItem("Choisissez un support…", "")
            for path, label in devices:
                self.device_selector.addItem(label, path)
            self.erase_status.set_content("SUPPORTS DÉTECTÉS", "success")
            self.erase_detail.setText("Sélectionnez le support à analyser.")
        else:
            self.device_selector.addItem("Aucun support détecté", "")
            self.erase_status.set_content("AUCUN SUPPORT", "warning")
            self.erase_detail.setText(
                "Branchez un support puis relancez la recherche, ou utilisez les options avancées."
            )
        self.device_selector.blockSignals(False)
        self.erase_target.clear()

    def select_device(self, index):
        path = self.device_selector.itemData(index) or ""
        self.erase_target.setText(path)

    def _update_erase_authorization(self, unused=None):
        self.erase_button.setEnabled(False)
        snapshot = self.preparation_snapshot
        if snapshot is None or self.process is not None:
            return
        try:
            if self.erase_target.text().strip() != snapshot["target"]:
                return
            if self.erase_method.currentText() != snapshot["method"]:
                return
            if self.erase_verification.currentText() != snapshot["verification"]:
                return
            identifier = snapshot["inspection"]["Identifiant"]
            if self.entered_identifier.text() != identifier[-6:]:
                return
            build_erase_command(
                snapshot["binary"],
                snapshot["target"],
                identifier,
                snapshot["method"],
                snapshot["verification"],
                self.erase_log.text(),
                self.operator.text(),
                self.witness.text(),
                self.acknowledgment.isChecked(),
            )
        except (KeyError, ValueError):
            return
        self.erase_button.setEnabled(True)

    def invalidate_preparation(self, unused=None):
        self.preparation_snapshot = None
        self.pending_inspection = None
        self.confirmed_target.setText("—")
        self.observed_identifier.setText("—")
        self.erase_button.setEnabled(False)
        self.prepare_button.setEnabled(
            bool(self.erase_target.text().strip()) and self.process is None
        )
        self.preparation_pill.set_content("ANALYSE REQUISE", "neutral")
        self.erase_status.set_content("EN ATTENTE DE PRÉPARATION", "neutral")
        self.erase_detail.setText("L’analyse est non destructive.")
        self.status_area.hide()
        self.selection_section.show()
        self.section_confirmation.hide()
        self._activate_step(1)

    def _erase_context(self):
        return (
            self._resolve_binary(),
            validated_text(self.erase_target.text(), "La cible"),
            validated_choice(self.erase_method.currentText(), METHODS, "La méthode"),
            validated_choice(
                self.erase_verification.currentText(), VERIFICATIONS, "La vérification"
            ),
        )

    def _activate_step(self, number):
        for index, step in enumerate(
            (self.target_step, self.identity_step, self.execution_step), 1
        ):
            step.setProperty("active", index == number)
            refresh_style(step)

    def prepare_erase(self):
        self.invalidate_preparation()
        try:
            context = self._erase_context()
            command = build_version_command(context[0])
        except ValueError as error:
            self._show_error(error)
            return
        self.status_area.show()
        self.erase_status.set_content("CONTRÔLE DU BINAIRE", "blue")
        self.erase_detail.setText(
            "Vérification de la version standard avant toute inspection."
        )
        self.run_command(
            command,
            self.erase_console,
            lambda code, output, error: self._after_preparation_version(
                context, code, output, error
            ),
        )

    def _after_preparation_version(self, context, code, output, error):
        if error or code != 0:
            self.erase_status.set_content("BINAIRE NON VÉRIFIABLE", "danger")
            return
        version = output.strip()
        if "-lab" in version:
            self.erase_status.set_content("BINAIRE LABORATOIRE REFUSÉ", "danger")
            self._show_error(
                "Le parcours d’effacement refuse les binaires marqués -lab."
            )
            return
        self.engine_pill.set_content(f"STANDARD {version}", "success")
        self.erase_status.set_content("INSPECTION EN COURS", "blue")
        self.run_command(
            build_inspect_command(context[0], context[1]),
            self.erase_console,
            lambda inspection_code, result, inspection_error: (
                self._after_preparation_inspection(
                    context,
                    inspection_code,
                    result,
                    inspection_error,
                )
            ),
            clear_output=False,
        )

    def _after_preparation_inspection(self, context, code, output, error):
        if error or code != 0:
            self.erase_status.set_content("INSPECTION REFUSÉE", "danger")
            return
        try:
            self.pending_inspection = parse_inspection(output)
        except ValueError as exception:
            self.erase_status.set_content("IDENTITÉ INEXPLOITABLE", "danger")
            self._show_error(exception)
            return
        self.erase_status.set_content("PLANIFICATION EN COURS", "blue")
        self.run_command(
            build_plan_command(context[0], context[1], context[2], context[3]),
            self.erase_console,
            lambda plan_code, result, plan_error: self._after_preparation_plan(
                context,
                plan_code,
                result,
                plan_error,
            ),
            clear_output=False,
        )

    def _after_preparation_plan(self, context, code, output, error):
        if error or code != 0 or not plan_is_executable(output):
            self.pending_inspection = None
            self.erase_status.set_content("PLAN REFUSÉ", "danger")
            self.erase_detail.setText(
                "Le cœur n’autorise aucun effacement avec ce contexte."
            )
            return
        try:
            if context != self._erase_context():
                raise ValueError("Les paramètres ont changé pendant la préparation.")
        except ValueError as exception:
            self.pending_inspection = None
            self.invalidate_preparation()
            self._show_error(exception)
            return
        self.preparation_snapshot = {
            "binary": context[0],
            "target": context[1],
            "method": context[2],
            "verification": context[3],
            "inspection": self.pending_inspection,
        }
        self.pending_inspection = None
        identifier = self.preparation_snapshot["inspection"]["Identifiant"]
        self.confirmed_target.setText(self.preparation_snapshot["target"])
        self.observed_identifier.setText(identifier)
        self.entered_identifier.setPlaceholderText(f"Recopiez : {identifier[-6:]}")
        if not self.erase_log.text().strip():
            timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            destination = (
                Path.home()
                / "datafuckerr"
                / "rapports"
                / f"réinitialisation-{timestamp}.jsonl"
            )
            self.erase_log.setText(str(destination))
            self.automatic_log = True
            self.report_destination.setText(
                "Enregistrement automatique dans le dossier datafuckerr"
            )
        self.preparation_pill.set_content("PLAN VALIDÉ", "success")
        self.erase_status.set_content("CONFIRMATION REQUISE", "warning")
        self.erase_detail.setText(
            "Recopiez l’identifiant, renseignez la traçabilité puis acquittez la perte de données."
        )
        self._update_erase_authorization()
        self.selection_section.hide()
        self.section_confirmation.show()
        self._activate_step(2)
        self.entered_identifier.setFocus()

    def select_erase_log(self):
        path, unused = QFileDialog.getSaveFileName(
            self,
            "Choisir le journal d’audit distinct",
            "",
            "Journal JSON Lines (*.jsonl)",
        )
        if path:
            self.erase_log.setText(path)
            self.automatic_log = False
            self.report_destination.setText(f"Emplacement personnalisé : {path}")

    def execute_erase(self):
        if self.preparation_snapshot is None:
            self._show_error("Inspectez et planifiez d’abord la cible.")
            return
        snapshot = self.preparation_snapshot
        try:
            context = self._erase_context()
            expected = (
                snapshot["binary"],
                snapshot["target"],
                snapshot["method"],
                snapshot["verification"],
            )
            if context != expected:
                raise ValueError(
                    "La cible, le binaire ou la stratégie a changé. Recommencez la préparation."
                )
            identifier = validated_text(
                self.entered_identifier.text(), "L’identifiant recopié"
            )
            expected_identifier = snapshot["inspection"]["Identifiant"]
            if identifier != expected_identifier[-6:]:
                raise ValueError(
                    "Les caractères recopiés ne correspondent pas à l’identité observée."
                )
            if self.automatic_log:
                Path(self.erase_log.text()).expanduser().parent.mkdir(
                    parents=True, exist_ok=True
                )
            build_erase_command(
                snapshot["binary"],
                snapshot["target"],
                expected_identifier,
                snapshot["method"],
                snapshot["verification"],
                self.erase_log.text(),
                self.operator.text(),
                self.witness.text(),
                self.acknowledgment.isChecked(),
            )
        except ValueError as error:
            self._show_error(error)
            return
        self.erase_status.set_content("RÉINSPECTION FINALE", "warning")
        self.erase_detail.setText(
            "Tout écart d’identité annule immédiatement l’opération."
        )
        self._activate_step(3)
        self.run_command(
            build_inspect_command(snapshot["binary"], snapshot["target"]),
            self.erase_console,
            lambda code, output, error: self._after_final_reinspection(
                snapshot, code, output, error
            ),
            clear_output=False,
        )

    def _create_final_confirmation(self, target, identifier):
        message = QMessageBox(self)
        message.setIcon(QMessageBox.Icon.Warning)
        message.setWindowTitle("Confirmation irréversible")
        message.setText("Dernière confirmation avant effacement")
        message.setInformativeText(
            f"Cible : {target}\n\nIdentifiant : {identifier}\n\nCette action est irréversible."
        )
        message.setStandardButtons(
            QMessageBox.StandardButton.Cancel | QMessageBox.StandardButton.Yes
        )
        message.setDefaultButton(QMessageBox.StandardButton.Cancel)
        cancel_button = message.button(QMessageBox.StandardButton.Cancel)
        cancel_button.setText("Annuler")
        cancel_button.setProperty("role", "secondary")
        confirm_button = message.button(QMessageBox.StandardButton.Yes)
        confirm_button.setText("Effacer maintenant")
        confirm_button.setProperty("role", "danger")
        configure_accessibility(confirm_button, "Confirmer l’effacement irréversible")
        refresh_style(cancel_button)
        refresh_style(confirm_button)
        return message

    def _after_final_reinspection(self, snapshot, code, output, error):
        if error or code != 0:
            self.invalidate_preparation()
            self.erase_status.set_content("RÉINSPECTION ÉCHOUÉE", "danger")
            return
        try:
            current_inspection = parse_inspection(output)
            if inspection_signature(current_inspection) != inspection_signature(
                snapshot["inspection"]
            ):
                raise ValueError(
                    "L’identité ou l’état de la cible a changé. Effacement annulé."
                )
            if self.entered_identifier.text() != current_inspection["Identifiant"][-6:]:
                raise ValueError(
                    "La confirmation ne correspond plus à l’identité réinspectée."
                )
            command = build_erase_command(
                snapshot["binary"],
                snapshot["target"],
                current_inspection["Identifiant"],
                snapshot["method"],
                snapshot["verification"],
                self.erase_log.text(),
                self.operator.text(),
                self.witness.text(),
                self.acknowledgment.isChecked(),
            )
        except ValueError as exception:
            self.invalidate_preparation()
            self._show_error(exception)
            return
        message = self._create_final_confirmation(
            snapshot["target"], current_inspection["Identifiant"]
        )
        if message.exec() != QMessageBox.StandardButton.Yes:
            self.erase_status.set_content("ANNULÉ PAR L’OPÉRATEUR", "neutral")
            return
        self.erase_status.set_content("EFFACEMENT EN COURS", "danger")
        self.erase_detail.setText(
            "Ne débranchez pas la cible. Surveillez le journal de session."
        )
        self.run_command(
            command, self.erase_console, self._after_erase, clear_output=False
        )

    def _after_erase(self, code, output, error):
        self.preparation_snapshot = None
        self.erase_button.setEnabled(False)
        if error:
            self.erase_status.set_content("ERREUR DE LANCEMENT", "danger")
        elif code == 0:
            self.erase_status.set_content("COMMANDE TERMINÉE", "success")
            self.erase_detail.setText(
                "Vérifiez maintenant le journal d’audit et générez le rapport technique."
            )
        elif code == 4:
            self.erase_status.set_content("ÉTAT INDÉTERMINÉ", "danger")
            self.erase_detail.setText("Mettez immédiatement la cible en quarantaine.")
            self._show_error(
                "État indéterminé : mettez immédiatement la cible en quarantaine."
            )
        else:
            self.erase_status.set_content("REFUSÉ OU ÉCHOUÉ", "danger")
            self.erase_detail.setText(
                "Consultez la sortie et le journal avant toute nouvelle action."
            )

    def select_audit(self):
        path, unused = QFileDialog.getOpenFileName(
            self,
            "Choisir un journal JSON Lines",
            "",
            "Journal JSON Lines (*.jsonl);;Tous les fichiers (*)",
        )
        if path:
            self.audit_path.setText(path)
            self.audit_pill.set_content("NON VÉRIFIÉ", "neutral")
            self.report_button.setEnabled(False)

    def verify_audit(self):
        try:
            command = build_verify_audit_command(
                self._resolve_binary(), self.audit_path.text()
            )
        except ValueError as error:
            self._show_error(error)
            return
        self.audit_pill.set_content("VÉRIFICATION", "blue")
        self.run_command(command, self.audit_console, self._after_audit_verification)

    def _after_audit_verification(self, code, output, error):
        if error or code != 0:
            self.audit_pill.set_content("JOURNAL REFUSÉ", "danger")
            self.audit_fingerprint.setText(
                "La chaîne ou la structure n’a pas été acceptée."
            )
            self.report_button.setEnabled(False)
            return
        fingerprint = ""
        for row in output.splitlines():
            if row.startswith("Empreinte finale :"):
                fingerprint = row.split(":", 1)[1].strip()
                break
        self.audit_pill.set_content("JOURNAL VALIDE", "success")
        self.audit_fingerprint.setText(
            fingerprint or "Journal accepté ; empreinte non extraite de la sortie."
        )
        self.report_button.setEnabled(True)

    def generate_report(self):
        try:
            binary = self._resolve_binary()
            audit = validated_text(self.audit_path.text(), "Le journal d’audit")
            command = build_verify_audit_command(binary, audit)
        except ValueError as error:
            self._show_error(error)
            return
        self.audit_pill.set_content("PRÉVÉRIFICATION", "blue")
        self.run_command(
            command,
            self.audit_console,
            lambda code, output, error: self._after_report_precheck(
                binary, audit, code, output, error
            ),
        )

    def _after_report_precheck(self, binary, audit, code, output, error):
        if error or code != 0:
            self.audit_pill.set_content("RAPPORT REFUSÉ", "danger")
            return
        destination, unused = QFileDialog.getSaveFileName(
            self,
            "Créer le rapport technique non certifié",
            str(Path(audit).with_suffix(".pdf")),
            "Document PDF (*.pdf)",
        )
        if not destination:
            self.audit_pill.set_content("GÉNÉRATION ANNULÉE", "neutral")
            return
        script = (
            Path(__file__).resolve().parent.parent
            / "tools"
            / "report"
            / "generate_report.py"
        )
        command = [
            sys.executable,
            str(script),
            audit,
            "--diskpurge",
            binary,
            "--output",
            destination,
        ]
        self.audit_pill.set_content("GÉNÉRATION PDF", "blue")
        self.run_command(
            command,
            self.audit_console,
            self._after_report_generation,
            clear_output=False,
        )

    def _after_report_generation(self, code, output, error):
        if error or code != 0:
            self.audit_pill.set_content("GÉNÉRATION ÉCHOUÉE", "danger")
        else:
            self.audit_pill.set_content("RAPPORT CRÉÉ", "success")

    def adjust_scale(self, delta):
        self.scale = min(1.35, max(0.9, round(self.scale + delta, 2)))
        self._apply_theme()

    def reset_scale(self):
        self.scale = 1.0
        self._apply_theme()

    def toggle_contrast(self):
        self.high_contrast = not self.high_contrast
        self.contrast_button.setChecked(self.high_contrast)
        state = "activé" if self.high_contrast else "désactivé"
        self.contrast_button.setAccessibleDescription(
            f"Contraste renforcé {state}. Raccourci Contrôle Majuscule C"
        )
        self._apply_theme()

    def _apply_theme(self):
        self.setStyleSheet(build_stylesheet(self.scale, self.high_contrast))

    def closeEvent(self, event):
        if self.process is not None:
            QMessageBox.warning(
                self,
                "Commande en cours",
                "La fenêtre reste ouverte pendant la commande. Une fermeture forcée peut laisser un état indéterminé.",
            )
            event.ignore()
            return
        event.accept()


def build_stylesheet(scale=1.0, high_contrast=False):
    background = "#FFFFFF"
    panel = "#FFFFFF"
    card = "#FFFFFF"
    secondary = "#FFFFFF" if high_contrast else "#F5F5F5"
    border = "#1B1F23" if high_contrast else "#E5E5E5"
    entry = "#1B1F23" if high_contrast else "#D4D4D8"
    text = "#000000" if high_contrast else "#0A0A0A"
    muted = "#303030" if high_contrast else "#737373"
    primary = "#000000" if high_contrast else "#171717"
    primary_text = "#FFFFFF" if high_contrast else "#FAFAFA"
    focus_ring = "#000000" if high_contrast else "#A3A3A3"
    danger = "#A80000" if high_contrast else "#E7000B"
    danger_hover = "#820000" if high_contrast else "#C9000A"
    hover = "#F0F0F0" if high_contrast else "#F5F5F5"

    def px(value):
        return max(8, int(round(value * scale)))

    return f"""
        * {{
            font-family: "SF Pro Text", "Segoe UI Variable", "Segoe UI", "Noto Sans", sans-serif;
            color: {text};
            font-size: {px(14)}px;
        }}
        QMainWindow, QWidget#Pages, QFrame#Content {{ background: {background}; }}
        QFrame#MainNavigation {{ background: {panel}; border-bottom: 1px solid {border}; }}
        QFrame#Topbar {{ background: {panel}; border-bottom: 1px solid {border}; }}
        QFrame#EngineCard {{ background: {card}; }}
        QLabel#Logo {{
            background: {primary}; color: {primary_text}; border-radius: {px(8)}px;
            font-size: {px(15)}px; font-weight: 700;
        }}
        QLabel[role="brand"] {{ font-size: {px(17)}px; font-weight: 600; }}
        QLabel[role="brandSub"] {{ color: {muted}; font-size: {px(11)}px; }}
        QLabel[role="navSection"], QLabel[role="eyebrow"], QLabel[role="eyebrowAccent"] {{
            color: {muted}; font-size: {px(12)}px; font-weight: 600;
        }}
        QLabel[role="breadcrumb"] {{ color: {muted}; font-size: {px(12)}px; font-weight: 500; }}
        QLabel[role="pageTitle"] {{ font-size: {px(28)}px; font-weight: 600; }}
        QLabel[role="pageLead"] {{ color: {muted}; font-size: {px(13)}px; }}
        QLabel[role="sectionTitle"] {{ font-size: {px(16)}px; font-weight: 600; }}
        QLabel[role="fieldLabel"] {{ color: {text}; font-size: {px(13)}px; font-weight: 500; }}
        QLabel[role="body"] {{ font-size: {px(14)}px; }}
        QLabel[role="bodyStrong"] {{ font-size: {px(14)}px; font-weight: 500; }}
        QLabel[role="muted"] {{ color: {muted}; font-size: {px(13)}px; }}
        QLabel[role="micro"] {{ color: {muted}; font-size: {px(11)}px; }}
        QLabel[role="mono"] {{ color: {text}; font-family: "Cascadia Mono", "SFMono-Regular", "Noto Sans Mono"; font-size: {px(12)}px; }}
        QLabel[role="metricValue"] {{ font-size: {px(18)}px; font-weight: 600; }}
        QLabel[role="bannerTitle"] {{ font-size: {px(14)}px; font-weight: 600; }}
        QFrame[role="card"], QFrame[role="metric"] {{
            background: {card}; border: 1px solid {border}; border-radius: {px(12)}px;
        }}
        QFrame[role="cardCompact"] {{ background: {card}; border: 1px solid {border}; border-radius: {px(10)}px; }}
        QFrame[role="status"] {{ background: {secondary}; border: 1px solid {border}; border-radius: {px(8)}px; }}
        QFrame#SafetyBanner {{
            background: {card}; border: 1px solid {border}; border-radius: {px(10)}px;
        }}
        QLabel#SafetyIcon {{ background: {secondary}; color: {text}; border-radius: {px(6)}px; font-size: {px(16)}px; font-weight: 600; }}
        QPushButton {{
            min-height: {px(36)}px; padding: 0 {px(14)}px; border-radius: {px(7)}px;
            background: {card}; border: 1px solid {border}; font-weight: 500;
        }}
        QPushButton:hover {{ background: {hover}; }}
        QPushButton:pressed {{ background: #E5E5E5; }}
        QPushButton:focus {{ border: 2px solid {focus_ring}; }}
        QPushButton:disabled {{ color: #A3A3A3; background: #FAFAFA; border-color: {border}; }}
        QPushButton[role="nav"] {{
            background: transparent; border: 1px solid transparent;
            color: {muted}; padding: 0 {px(14)}px; border-radius: {px(7)}px;
        }}
        QPushButton[role="nav"]:hover {{ color: {text}; background: {secondary}; }}
        QPushButton[role="nav"]:checked {{ color: {text}; background: {secondary}; border-color: {border}; }}
        QPushButton[role="primary"] {{
            background: {primary}; color: {primary_text}; border-color: {primary};
            min-height: {px(36)}px; font-weight: 500;
        }}
        QPushButton[role="primary"]:hover {{ background: #262626; border-color: #262626; }}
        QPushButton[role="primary"]:pressed {{ background: #0A0A0A; }}
        QPushButton[role="primary"]:disabled {{ background: #D4D4D4; color: #FAFAFA; border-color: #D4D4D4; }}
        QPushButton[role="secondary"] {{ background: {secondary}; border-color: {secondary}; }}
        QPushButton[role="ghost"], QPushButton[role="toolbar"] {{ background: transparent; border-color: transparent; min-height: {px(34)}px; padding: 0 12px; }}
        QPushButton[role="toolbar"] {{ min-width: {px(34)}px; }}
        QPushButton[role="toolbar"]:checked {{ background: {secondary}; border-color: {border}; }}
        QPushButton[role="link"] {{
            background: transparent; border: none; color: {text}; min-height: {px(28)}px;
            padding: 0 4px; text-align: left; text-decoration: none;
        }}
        QPushButton[role="link"]:hover {{ background: transparent; text-decoration: underline; }}
        QPushButton[role="danger"] {{
            background: {danger}; color: #FFFFFF; border-color: {danger}; min-height: {px(40)}px;
            border-radius: {px(7)}px; font-weight: 500;
        }}
        QPushButton[role="danger"]:hover {{ background: {danger_hover}; border-color: {danger_hover}; }}
        QPushButton[role="danger"]:pressed {{ background: #A00008; }}
        QPushButton[role="danger"]:disabled {{ background: #FAFAFA; color: #A3A3A3; border-color: {border}; }}
        QPushButton[role="actionCard"], QPushButton[role="actionCardDanger"] {{
            text-align: left; min-height: {px(44)}px; padding: 0 16px; background: {card};
        }}
        QPushButton[role="actionCard"]:hover {{ background: {secondary}; }}
        QPushButton[role="actionCardDanger"] {{ color: {danger}; }}
        QPushButton[role="actionCardDanger"]:hover {{ background: #FEF2F2; border-color: {danger}; }}
        QLabel[role="pill"] {{
            min-height: {px(22)}px; padding: 0 8px; border-radius: {px(11)}px;
            font-size: {px(10)}px; font-weight: 600;
            background: {secondary}; color: {text}; border: 1px solid {border};
        }}
        QLabel[role="pill"][tone="success"] {{ background: #F0FDF4; color: #166534; border-color: #BBF7D0; }}
        QLabel[role="pill"][tone="blue"] {{ background: {secondary}; color: {text}; border-color: {border}; }}
        QLabel[role="pill"][tone="warning"] {{ background: #FFFBEB; color: #92400E; border-color: #FDE68A; }}
        QLabel[role="pill"][tone="danger"] {{ background: #FEF2F2; color: #991B1B; border-color: #FECACA; }}
        QLabel[role="controlNumber"], QLabel[role="stepNumber"] {{
            background: {secondary}; color: {text}; border: 1px solid {border}; border-radius: {px(14)}px; font-weight: 600;
        }}
        QFrame[role="step"][active="true"] QLabel[role="stepNumber"] {{
            background: {primary}; color: {primary_text}; border-color: {primary};
        }}
        QLabel[role="checkIcon"] {{ background: {secondary}; color: {text}; border-radius: {px(6)}px; font-weight: 600; }}
        QFrame[role="step"] {{ background: transparent; border: none; }}
        QFrame[role="stepSeparator"] {{ background: {border}; border: none; min-height: 1px; max-height: 1px; }}
        QLineEdit, QComboBox, QPlainTextEdit, QTableWidget {{
            background: {card}; border: 1px solid {entry}; border-radius: {px(7)}px;
            selection-background-color: {primary}; selection-color: {primary_text};
        }}
        QLineEdit, QComboBox {{ min-height: {px(36)}px; padding: 0 12px; }}
        QLineEdit:hover, QComboBox:hover {{ border-color: {focus_ring}; }}
        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus, QTableWidget:focus {{ border: 2px solid {focus_ring}; }}
        QLineEdit:read-only {{ color: {text}; background: {secondary}; }}
        QComboBox QAbstractItemView {{ background: {card}; color: {text}; selection-background-color: {secondary}; selection-color: {text}; }}
        QPlainTextEdit {{
            padding: 12px; background: #0A0A0A; border-color: #0A0A0A; border-radius: {px(8)}px;
            font-family: "Cascadia Mono", "SFMono-Regular", "Noto Sans Mono";
            font-size: {px(11)}px; color: #FAFAFA;
        }}
        QTableWidget {{ gridline-color: {border}; alternate-background-color: #FAFAFA; }}
        QHeaderView::section {{ background: {secondary}; color: {text}; border: none; border-right: 1px solid {border}; border-bottom: 1px solid {border}; padding: 8px; font-weight: 500; }}
        QCheckBox {{ spacing: 10px; color: {text}; }}
        QScrollArea {{ background: transparent; border: none; }}
        QScrollArea#EffacementScroll, QWidget#EffacementViewport, QWidget#EffacementContenu {{ background: {background}; }}
        QScrollBar:vertical {{ background: transparent; width: 9px; margin: 2px; }}
        QScrollBar::handle:vertical {{ background: #D4D4D4; border-radius: 3px; min-height: 32px; }}
        QScrollBar::handle:vertical:hover {{ background: #A3A3A3; }}
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{ height: 0; }}
        QMessageBox {{ background: {card}; }}
        QToolTip {{ background: {primary}; color: {primary_text}; border: 1px solid {primary}; padding: 4px; }}
    """


def build_parser():
    parser = argparse.ArgumentParser(description="Interface graphique datafuckerr Qt 6")
    parser.add_argument(
        "binary", nargs="?", help="chemin du binaire diskpurge standard"
    )
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    application = QApplication(sys.argv[:1])
    application.setApplicationName("datafuckerr")
    application.setApplicationDisplayName("datafuckerr — Centre de sanitisation")
    application.setOrganizationName("datafuckerr")
    application.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.GeneralFont))
    window = DatafuckerrWindow(args.binary)
    window.show()
    return application.exec()


if __name__ == "__main__":
    raise SystemExit(main())
