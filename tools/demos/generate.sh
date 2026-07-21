#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
binary="$root/build/diskpurge"
output="$root/demos"
font="/System/Library/Fonts/SFNSMono.ttf"

if [ ! -x "$binary" ]; then
    make -C "$root"
fi

if [ ! -f "$font" ]; then
    font="/System/Library/Fonts/Monaco.ttf"
fi

temporary=$(mktemp -d "${TMPDIR:-/tmp}/diskpurge-gif.XXXXXX")
trap 'rm -rf "$temporary"' EXIT
mkdir -p "$output"

frame_number=0
session=""
frames=""
durations=""

reset_demo() {
    title="$1"
    work="$temporary/$2"
    rm -rf "$work"
    mkdir -p "$work/frames"
    session="$work/session.txt"
    frames="$work/frames"
    durations="$work/durations.txt"
    frame_number=0
    printf '%s\n\n' "$title" > "$session"
    snapshot 1.2
}

snapshot() {
    duration="$1"
    frame_number=$((frame_number + 1))
    frame=$(printf '%s/%03d.txt' "$frames" "$frame_number")
    wrapped="$work/wrapped.txt"
    fold -s -w 96 "$session" > "$wrapped"
    line_count=$(wc -l < "$wrapped" | tr -d ' ')
    if [ "$line_count" -gt 21 ]; then
        tail -n 21 "$wrapped" > "$frame"
    else
        cp "$wrapped" "$frame"
    fi
    printf '%s\t%s\n' "$frame" "$duration" >> "$durations"
}

prompt() {
    printf '\n❯ %s\n' "$1" >> "$session"
    snapshot 0.8
}

append_file() {
    sed "s|$temporary|/tmp/diskpurge-demo|g" "$1" >> "$session"
    snapshot "$2"
}

render_demo() {
    name="$1"
    title_text="$2"
    concat="$work/concat.txt"
    : > "$concat"
    tab=$(printf '\tX')
    tab=${tab%X}
    while IFS="$tab" read -r text duration; do
        base=$(basename "$text" .txt)
        png="$frames/$base.png"
        ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=0x0d1117:s=1200x700:d=0.1" \
            -vf "drawbox=x=0:y=0:w=iw:h=54:color=0x161b22:t=fill,drawbox=x=22:y=20:w=14:h=14:color=0xff5f56:t=fill,drawbox=x=46:y=20:w=14:h=14:color=0xffbd2e:t=fill,drawbox=x=70:y=20:w=14:h=14:color=0x27c93f:t=fill,drawtext=fontfile='$font':text='$title_text':fontcolor=0xc9d1d9:fontsize=20:x=100:y=14,drawtext=fontfile='$font':textfile='$text':fontcolor=0xd2f8d2:fontsize=19:x=32:y=76:line_spacing=8:expansion=none" \
            -frames:v 1 "$png"
        printf "file '%s'\nduration %s\n" "$png" "$duration" >> "$concat"
    done < "$durations"
    last_png=$(tail -n 1 "$durations" | cut -f1 | sed 's/\.txt$/.png/')
    printf "file '%s'\n" "$last_png" >> "$concat"
    ffmpeg -hide_banner -loglevel error -f concat -safe 0 -i "$concat" \
        -vf "fps=12,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer" \
        -loop 0 -y "$output/$name.gif"
}

virtual="$temporary/disque-virtuel.img"
audit="$temporary/audit.jsonl"
dd if=/dev/urandom of="$virtual" bs=1m count=16 2>/dev/null

reset_demo "DISKPURGE 0.2 — PARCOURS SÉCURISÉ" flow
prompt "diskpurge --version"
"$binary" --version > "$work/result.txt"
append_file "$work/result.txt" 1.0
prompt "diskpurge inspect /tmp/diskpurge-demo/disque-virtuel.img"
"$binary" inspect "$virtual" > "$work/result.txt"
append_file "$work/result.txt" 1.8
identifier=$(sed -n 's/^Identifiant  : //p' "$work/result.txt")
prompt "diskpurge plan /tmp/diskpurge-demo/disque-virtuel.img --method auto"
"$binary" plan "$virtual" --method auto > "$work/result.txt"
append_file "$work/result.txt" 1.8
prompt "diskpurge erase … --confirm $identifier --verify full --audit audit.jsonl"
"$binary" erase "$virtual" --confirm "$identifier" --verify full --audit "$audit" > "$work/result.txt" 2>&1
tr '\r' '\n' < "$work/result.txt" | awk 'NF {line=$0} END {print line}' > "$work/compact.txt"
append_file "$work/compact.txt" 1.4
prompt "grep -o '\"statut\":\"[^\"]*\"' audit.jsonl"
grep -o '"statut":"[^"]*"' "$audit" | tail -n 1 > "$work/result.txt"
append_file "$work/result.txt" 2.5
render_demo "01-parcours-securise" "diskpurge · parcours sécurisé"

dd if=/dev/urandom of="$virtual" bs=1m count=16 conv=notrunc 2>/dev/null
reset_demo "DISKPURGE 0.2 — GARDE-FOUS" guards
prompt "diskpurge erase disque-virtuel.img --confirm mauvais-identifiant"
"$binary" erase "$virtual" --confirm mauvais-identifiant > "$work/result.txt" 2>&1 || true
append_file "$work/result.txt" 1.8
prompt "diskpurge plan disque-virtuel.img --method purge"
"$binary" plan "$virtual" --method purge > "$work/result.txt" 2>&1 || true
append_file "$work/result.txt" 1.8
prompt "diskpurge plan disque-virtuel.img --method destroy"
"$binary" plan "$virtual" --method destroy > "$work/result.txt" 2>&1 || true
append_file "$work/result.txt" 2.0
prompt "diskpurge plan disque-virtuel.img --method pruge"
"$binary" plan "$virtual" --method pruge > "$work/result.txt" 2>&1 || true
append_file "$work/result.txt" 2.5
render_demo "02-garde-fous" "diskpurge · refus contrôlés"

speed_disk="$temporary/test-128m.img"
dd if=/dev/urandom of="$speed_disk" bs=1m count=128 2>/dev/null
reset_demo "DISKPURGE 0.2 — TEST VIRTUEL ACCÉLÉRÉ" speed
printf 'Support virtuel : 128 Mio en cache\nMesure illustrative, non représentative d’un HDD ou SSD réel.\n' >> "$session"
snapshot 1.8
prompt "diskpurge inspect /tmp/diskpurge-demo/test-128m.img"
"$binary" inspect "$speed_disk" > "$work/result.txt"
append_file "$work/result.txt" 1.6
speed_id=$(sed -n 's/^Identifiant  : //p' "$work/result.txt")
prompt "time diskpurge erase test-128m.img --confirm $speed_id --verify full"
{ /usr/bin/time -p "$binary" erase "$speed_disk" --confirm "$speed_id" --verify full; } > "$work/result.txt" 2>&1
tr '\r' '\n' < "$work/result.txt" | awk '/Effacement/ {progress=$0} /terminés/ || /^real / || /^user / || /^sys / {print} END {if (progress != "") print progress}' > "$work/compact.txt"
append_file "$work/compact.txt" 2.0
printf '\n✓ 134 217 728 octets écrits puis intégralement vérifiés.\n' >> "$session"
snapshot 3.0
render_demo "03-test-virtuel-accelere" "diskpurge · banc virtuel"

printf 'GIF générés dans %s\n' "$output"
