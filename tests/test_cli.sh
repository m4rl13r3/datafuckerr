#!/bin/sh
set -eu

binary="$1"
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
expected_version="${2:-$(sed -n '1p' "$root/VERSION")}"
temporary="${TMPDIR:-/tmp}/diskpurge-test-$$.img"
symbolic_link="$temporary.link"
audit_symbolic_link="$temporary.audit.link"
audit_hardlink="$temporary.audit.hardlink"
hardlink_audit="$temporary.hardlink"
empty_image="$temporary.empty"
non_aligned="$temporary.nonaligned"
trap 'rm -f "$temporary" "$temporary.audit" "$temporary.audit.tampered" "$temporary.audit.incomplete" "$temporary.audit.suffix" "$temporary.audit.interposed" "$temporary.audit.empty" "$empty_image" "$non_aligned" "$symbolic_link" "$audit_symbolic_link" "$audit_hardlink" "$hardlink_audit"' EXIT

dd if=/dev/urandom of="$temporary" bs=4096 count=4 2>/dev/null
identifier="$($binary inspect "$temporary" | sed -n 's/^Identifiant  : //p')"
ln -s "$temporary" "$symbolic_link"
if $binary inspect "$symbolic_link" >/dev/null 2>&1; then
    exit 1
fi

test "$($binary --version)" = "$expected_version"
$binary plan "$temporary" | grep -q 'Exécutable   : oui'
$binary plan "$temporary" | grep -q 'Méthode      : clear-zero'
if $binary plan "$temporary" --method pruge >/dev/null 2>&1; then
    exit 1
fi
if $binary inspect "$temporary" --option-inconnue >/dev/null 2>&1; then
    exit 1
fi
if $binary plan "$temporary" --verify valeur-invalide >/dev/null 2>&1; then
    exit 1
fi
if $binary plan "$temporary" --lab-mode >/dev/null 2>&1; then
    exit 1
fi
if $binary plan "$temporary" --verify full --verify sample >/dev/null 2>&1; then
    exit 1
fi
if $binary erase "$temporary" --confirm incorrect 2>/dev/null; then
    exit 1
fi
$binary erase "$temporary" --confirm "$identifier" --verify full --audit "$temporary.audit" >/dev/null 2>/dev/null
test "$(od -v -An -tu1 "$temporary" | tr -d ' 0\n')" = ""
grep -q '"statut":"réussi"' "$temporary.audit"
grep -Eq '"opération":"[0-9a-f]{64}"' "$temporary.audit"
grep -Eq '"empreinte":"[0-9a-f]{64}"' "$temporary.audit"
$binary verify-audit "$temporary.audit" | grep -q 'Journal valide : 2 enregistrements'
ln -s "$temporary.audit" "$audit_symbolic_link"
if $binary verify-audit "$audit_symbolic_link" >/dev/null 2>&1; then
    exit 1
fi
ln "$temporary.audit" "$audit_hardlink"
if $binary verify-audit "$audit_hardlink" >/dev/null 2>&1; then
    exit 1
fi
rm -f "$audit_hardlink"
sed 's/"statut":"réussi"/"statut":"falsifié"/' "$temporary.audit" > "$temporary.audit.tampered"
if $binary verify-audit "$temporary.audit.tampered" >/dev/null 2>&1; then
    exit 1
fi
if $binary erase "$temporary" --confirm "$identifier" --verify full --audit "$temporary.audit.tampered" >/dev/null 2>&1; then
    exit 1
fi
head -n 1 "$temporary.audit" > "$temporary.audit.incomplete"
if $binary verify-audit "$temporary.audit.incomplete" >/dev/null 2>&1; then
    exit 1
fi
sed 's/$/suffixe-interdit/' "$temporary.audit" > "$temporary.audit.suffix"
if $binary verify-audit "$temporary.audit.suffix" >/dev/null 2>&1; then
    exit 1
fi
sed 's/","empreinte"/","champ_inattendu":1,"empreinte"/' "$temporary.audit" > "$temporary.audit.interposed"
if $binary verify-audit "$temporary.audit.interposed" >/dev/null 2>&1; then
    exit 1
fi
: > "$temporary.audit.empty"
if $binary verify-audit "$temporary.audit.empty" >/dev/null 2>&1; then
    exit 1
fi

dd if=/dev/urandom of="$non_aligned" bs=8193 count=1 2>/dev/null
non_aligned_identifier="$($binary inspect "$non_aligned" | sed -n 's/^Identifiant  : //p')"
$binary erase "$non_aligned" --confirm "$non_aligned_identifier" --verify full >/dev/null 2>/dev/null
test "$(wc -c < "$non_aligned" | tr -d ' ')" = "8193"
test "$(od -v -An -tu1 "$non_aligned" | tr -d ' 0\n')" = ""

: > "$empty_image"
empty_identifier="$($binary inspect "$empty_image" | sed -n 's/^Identifiant  : //p')"
$binary erase "$empty_image" --confirm "$empty_identifier" --verify full >/dev/null 2>/dev/null
test ! -s "$empty_image"

dd if=/dev/urandom of="$temporary" bs=4096 count=1 conv=notrunc 2>/dev/null
current_identifier="$($binary inspect "$temporary" | sed -n 's/^Identifiant  : //p')"
before="$(cksum "$temporary")"
if $binary erase "$temporary" --confirm "$current_identifier" --audit "$temporary" >/dev/null 2>&1; then
    exit 1
fi
test "$(cksum "$temporary")" = "$before"
ln "$temporary" "$hardlink_audit"
if "$binary" erase "$temporary" --confirm "$current_identifier" --audit "$hardlink_audit" >/dev/null 2>&1; then
    exit 1
fi
test "$(cksum "$temporary")" = "$before"

printf 'Tests CLI réussis.\n'
