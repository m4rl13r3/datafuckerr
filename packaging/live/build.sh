#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work_dir=${DATAFUCKERR_LIVE_WORKDIR:-"$root/build/live"}
output_dir=${DATAFUCKERR_ISO_OUTPUT:-"$root/output/iso"}

if [ "$(uname -s)" != "Linux" ]; then
    echo "Erreur : la construction de l’ISO exige un hôte Linux ou le workflow GitHub dédié." >&2
    exit 1
fi
if [ "$(id -u)" -ne 0 ]; then
    echo "Erreur : live-build doit être lancé comme root, par exemple avec sudo." >&2
    exit 1
fi
if ! command -v lb >/dev/null 2>&1; then
    echo "Erreur : live-build est introuvable." >&2
    exit 1
fi

make -C "$root" clean
make -C "$root" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
version=$("$root/build/diskpurge" --version)
case "$version" in
    *-lab)
        echo "Erreur : un binaire laboratoire ne peut jamais entrer dans l’ISO." >&2
        exit 1
        ;;
esac
volume_label=$(printf '%s' "$version" | tr -cd 'A-Za-z0-9' | cut -c1-16)

case "$work_dir" in
    "$root"/build/live|"$root"/build/live/*) ;;
    *)
        echo "Erreur : le répertoire de travail doit rester sous build/live." >&2
        exit 1
        ;;
esac

rm -rf "$work_dir"
mkdir -p "$work_dir/config" "$work_dir/config/includes.chroot/opt/datafuckerr/ui"
cp -R "$root/packaging/live/config/." "$work_dir/config/"
cp "$root/build/diskpurge" "$work_dir/config/includes.chroot/opt/datafuckerr/diskpurge"
cp "$root/ui/datafuckerr_qt.py" "$work_dir/config/includes.chroot/opt/datafuckerr/ui/datafuckerr_qt.py"
cp "$root/ui/diskpurge_commands.py" "$work_dir/config/includes.chroot/opt/datafuckerr/ui/diskpurge_commands.py"
chmod 0755 "$work_dir/config/includes.chroot/opt/datafuckerr/diskpurge"

cd "$work_dir"
lb config \
    --mode debian \
    --distribution trixie \
    --architecture amd64 \
    --binary-image iso-hybrid \
    --bootloaders "grub-pc,grub-efi" \
    --archive-areas "main contrib non-free-firmware" \
    --apt-recommends true \
    --checksums sha256 \
    --compression xz \
    --debian-installer none \
    --firmware-binary true \
    --firmware-chroot true \
    --iso-application "datafuckerr Live $version" \
    --iso-publisher "Projet datafuckerr" \
    --iso-volume "datafuckerr_$volume_label" \
    --bootappend-live "boot=live components locales=fr_FR.UTF-8 keyboard-layouts=fr timezone=Europe/Paris"
lb build

iso_path=$(find "$work_dir" -maxdepth 1 -type f -name '*.iso' -print | head -n 1)
if [ -z "$iso_path" ]; then
    echo "Erreur : live-build n’a produit aucune ISO." >&2
    exit 1
fi
mkdir -p "$output_dir"
destination_path="$output_dir/datafuckerr-live-${version}-amd64.iso"
cp "$iso_path" "$destination_path"
(cd "$output_dir" && sha256sum "$(basename "$destination_path")" > "$(basename "$destination_path").sha256")
echo "ISO créée : $destination_path"
