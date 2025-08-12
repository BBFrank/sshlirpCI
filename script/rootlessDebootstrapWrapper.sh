#!/bin/bash

# Copyright Muxup contributors.
# Distributed under the terms of the MIT-0 license, see LICENSE for details.
# SPDX-License-Identifier: MIT-0

TARGET_DIR=""
SUITE=""
MIRROR=""
ARGSTR=""
ARCH=""
SUDO_USER_FLAG="0"
SUDO_CMD=""

error() {
  printf "!!!!!!!!!! Error: %s !!!!!!!!!!\n" "$*" >&2
  exit 1
}

usage_error() {
  usage
  error "$*"
}

error_if_whitespace() {
  case "$1" in
    *[[:space:]]*)
      error "Rejecting '$1' - Arguments with whitespace are not supported"
      ;;
  esac
}

usage() {
  echo "Usage: rootless-debootstrap-wrapper --target-dir TGT --suite SUITE [--mirror MIRROR] [--include INCLUDE] [--arch ARCH] [--sudo-user 0|1] [...passthrough_opts]"
}

if [ $# -eq 0 ]; then
  usage
  exit
fi

while [ $# -gt 0 ]; do
  error_if_whitespace "$1"
  case "$1" in
    --help|-h)
      usage
      exit
      ;;
    --include|--include=*)
      if [ "$1" = "--include" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        INCLUDE="$2,fakeroot"
        shift
      elif [ "${1#--include=}" != "$1" ]; then
        INCLUDE="${1#--include=},fakeroot"
      else
        usage_error "Option --include requires an argument"
      fi
      ARGSTR="$ARGSTR --include=$INCLUDE"
      ;;
    --mirror|--mirror=*)
      if [ "$1" = "--mirror" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        MIRROR="$2"
        shift
      elif [ "${1#--mirror=}" != "$1" ]; then
        MIRROR="${1#--mirror=}"
      else
        usage_error "Option --mirror requires an argument"
      fi
      ;;
    --suite|--suite=*)
      if [ "$1" = "--suite" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        SUITE="$2"
        shift
      elif [ "${1#--suite=}" != "$1" ]; then
        SUITE="${1#--suite=}"
      else
        usage_error "Option --suite requires an argument"
      fi
      ;;
    --target-dir|--target-dir=*)
      if [ "$1" = "--target-dir" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        TARGET_DIR="$2"
        shift
      elif [ "${1#--target-dir=}" != "$1" ]; then
        TARGET_DIR="${1#--target-dir=}"
      else
        usage_error "Option --target-dir requires an argument"
      fi
      ;;
    --arch|--arch=*)
      if [ "$1" = "--arch" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        ARCH="$2"
        shift
      elif [ "${1#--arch=}" != "$1" ]; then
        ARCH="${1#--arch=}"
      else
        usage_error "Option --arch requires an argument"
      fi
      ;;
    --sudo-user|--sudo-user=*)
      if [ "$1" = "--sudo-user" ] && [ "$2" ]; then
        error_if_whitespace "$2"
        SUDO_USER_FLAG="$2"
        shift
      elif [ "${1#--sudo-user=}" != "$1" ]; then
        SUDO_USER_FLAG="${1#--sudo-user=}"
      else
        usage_error "Option --sudo-user requires an argument (0|1)"
      fi
      case "$SUDO_USER_FLAG" in
        0|1) ;;
        *) usage_error "Invalid value for --sudo-user (use 0 or 1)";;
      esac
      ;;
    *)
      ARGSTR="$ARGSTR $1"
      ;;
  esac
  shift
done

[ -n "$SUITE" ] || usage_error "Must set --suite"
[ -n "$TARGET_DIR" ] || usage_error "Must set --target-dir"
[ ! -e "$TARGET_DIR" ] || error "Directory in --target-dir already exists, refusing to run"
[ -z "$INCLUDE" ] && ARGSTR="$ARGSTR --include=fakeroot"

[ -n "$ARCH" ] && ARCH_OPT="--arch=$ARCH"

if [ "$SUDO_USER_FLAG" = "1" ]; then
  SUDO_CMD="sudo"
  echo "@@@@@@@@@@ sudo-user=1: user requested sudo elevation for debootstrap stages @@@@@@@@@@"
else
  SUDO_CMD=""
fi

ARGSTR="--foreign $ARCH_OPT $ARGSTR $SUITE $TARGET_DIR"
[ -n "$MIRROR" ] && ARGSTR="$ARGSTR $MIRROR"

echo "@@@@@@@@@@ Starting first stage debootstrap (arch: ${ARCH:-host default}) @@@@@@@@@@"
TMP_FAKEROOT_ENV=$(mktemp)
fakeroot -s "$TMP_FAKEROOT_ENV" debootstrap $ARGSTR || error "Stage 1 debootstrap failed"
mv "$TMP_FAKEROOT_ENV" "$TARGET_DIR/.fakeroot.env"

echo "@@@@@@@@@@ Extracting fakeroot for target @@@@@@@@@@"
cd "$TARGET_DIR" || error "cd failed"
fakeroot -i .fakeroot.env -s .fakeroot.env bash -e <<'EOF' || error "Failed to extract fakeroot for target"
for deb in ./var/cache/apt/archives/{libfakeroot_,fakeroot_}*.deb; do
  tarball_ext=$(ar t "$deb" | sed -n '/^data\.tar\.[^.]*$/s/.*\.//p')
  case "$tarball_ext" in
    gz) decomp_flag=--gzip  ;;
    xz) decomp_flag=--xz    ;;
    zst) decomp_flag=--zstd ;;
    *) echo "Unknown extension for tarball $deb"; exit 1 ;;
  esac
  ar p "$deb" "data.tar.$tarball_ext" | tar x $decomp_flag
done
ln -s fakeroot-sysv ./usr/bin/fakeroot
EOF
cd "$OLDPWD" || error "cd failed"

# Prima la variabile USE_SUDO (espansione)
cat > "$TARGET_DIR/_enter" <<EOF
#!/bin/bash
set -e
USE_SUDO="$SUDO_USER_FLAG"
EOF

# Poi appendo il corpo con heredoc single-quoted (niente espansioni premature)
cat >> "$TARGET_DIR/_enter" <<'EOF'
export PATH=/usr/sbin:$PATH

# Percorso host del chroot, derivato dalla posizione di questo script
HOST_TARGET_DIR="$(cd "$(dirname "$0")" && pwd)"

# Rispetta l'opzione --sudo-user passata allo script wrapper
SUDO_CMD=""
if [ "$USE_SUDO" = "1" ]; then
  SUDO_CMD="sudo"
fi

# Bind /dev/net/ se disponibile (best effort)
# ----------------- commentabile -----------------
# Si tratta di una patch al problema del mknod nello script test.sh. Purtroppo però anche il bind necessita di privilegi root
# e quindi questa fase fallisce sempre con "[_enter] WARNING: Bind /dev/net/ fallito (continuo comunque).".
# Il programma termina comunque correttamente con i binari costruiti correttamente ma non testati.
# Nota: in quanto il bind comunque non funziona senza sudo, allora faccio direttamente mknod dal test.sh
# nel caso in cui sshlirp_ci_start venga lanciato con sudo, così evito di dover fare poi umount
# Nota 2: se si intende decommentare questa parte ed utilizzare il bind qui piuttosto che il mknod in test, allora assicurarsi che sia
# commentato il mknod
#if [ -e /dev/net/ ]; then
#  "$SUDO_CMD" mkdir -p "$HOST_TARGET_DIR/dev/net/"
#  if "$SUDO_CMD" mountpoint -q "$HOST_TARGET_DIR/dev/net/"; then
#    echo "[_enter] /dev/net/ already bound in the chroot."
#  else
#    if "$SUDO_CMD" mount --bind /dev/net/ "$HOST_TARGET_DIR/dev/net/"; then
#      echo "[_enter] Bind /dev/net/ -> chroot succeeded."
#    else
#      echo "[_enter] WARNING: Bind /dev/net/ failed (continuing anyway)." >&2
#    fi
#  fi
#else
#  echo "[_enter] /dev/net/ not present on host: skipping bind."
#fi
# ----------------- commentabile -----------------

# - con sudo: NO user namespace (-r)
# - senza sudo: usa -r per rootless
if [ "$USE_SUDO" = "1" ]; then
  FAKEROOTDONTTRYCHOWN=1 sudo unshare -fp -m --mount-proc -R "$(dirname -- "$0")" \
    fakeroot -i .fakeroot.env -s .fakeroot.env "$@"
else
  FAKEROOTDONTTRYCHOWN=1 unshare -fpr --mount-proc -R "$(dirname -- "$0")" \
    fakeroot -i .fakeroot.env -s .fakeroot.env "$@"
fi

EOF

$SUDO_CMD chmod +x "$TARGET_DIR/_enter"

echo "@@@@@@@@@@ Starting second stage debootstrap @@@@@@@@@@"
$SUDO_CMD "$TARGET_DIR/_enter" debootstrap/debootstrap --second-stage --keep-debootstrap-dir || error "Stage 2 debootstrap failed"
$SUDO_CMD mv "$TARGET_DIR/debootstrap/debootstrap.log" "$TARGET_DIR/_debootstrap.log"
$SUDO_CMD rm -rf "$TARGET_DIR/debootstrap"
echo "@@@@@@@@@@ Debootstrap complete! @@@@@@@@@@"