# Composants tiers des applications natives

Les applications autonomes datafuckerr incorporent un interpréteur Python, Qt 6 via PySide6 Essentials, ReportLab, Pillow et le chargeur PyInstaller. Le moteur `diskpurge` reste construit depuis les sources de ce dépôt sous licence Apache-2.0.

| Composant | Version de construction | Licence déclarée | Projet |
| --- | --- | --- | --- |
| Python | 3.12.x | PSF-2.0 | https://www.python.org/ |
| PySide6 Essentials et Qt 6 | 6.11.1 | LGPL-3.0-only, GPL-3.0-only ou commerciale selon les conditions de Qt | https://doc.qt.io/qtforpython-6/licenses.html |
| ReportLab | 4.5.1 | BSD-3-Clause | https://www.reportlab.com/ |
| Pillow | 12.3.0 | HPND | https://python-pillow.org/ |
| PyInstaller | 6.21.0 | GPL-2.0-or-later avec exception pour les applications distribuées | https://pyinstaller.org/ |

Les bibliothèques Qt restent chargées dynamiquement dans les paquets. L’AppImage peut être extraite avec `--appimage-extract`, le contenu d’une application macOS peut être consulté depuis le Finder et l’installation Windows conserve les DLL comme fichiers séparés. Ces formats ne doivent pas être transformés de manière à empêcher le remplacement des bibliothèques couvertes par la LGPL.

Le répertoire `licenses` de chaque application autonome contient les textes GPL-3.0, LGPL-3.0 et PSF-2.0 ainsi que les licences fournies par ReportLab, Pillow, PyInstaller et charset-normalizer. Les liens ci-dessus identifient les sources officielles correspondant aux composants redistribués.
