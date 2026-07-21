#!/bin/sh
set -eu
script_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
python_command=${PYTHON:-python3}
if ! command -v "$python_command" >/dev/null 2>&1; then
    echo "Erreur : Python 3 est introuvable." >&2
    exit 1
fi
if ! "$python_command" -c 'import sys, PySide6; raise SystemExit(sys.version_info < (3, 10))' >/dev/null 2>&1; then
    echo "Erreur : datafuckerr exige Python 3.10 ou supérieur et PySide6." >&2
    echo "Installez l’interface avec : $python_command -m pip install -r $script_directory/requirements.txt" >&2
    exit 1
fi
exec "$python_command" "$script_directory/datafuckerr_qt.py" "$@"
