#!/usr/bin/env bash
set -euo pipefail
# Explicit optional installation. No GPU setters are called by this script.
if [[ ${EUID} -ne 0 || $# -ne 1 || ! -f $1 ]]; then
    printf '%s\n' 'Usage: sudo bash install-gpu-control-helper.sh /absolute/path/openpuzzle-gpu-control' >&2
    exit 1
fi
source_helper=$(realpath -- "$1")
if [[ $(basename -- "$source_helper") != openpuzzle-gpu-control ]]; then
    printf '%s\n' 'Expected the dedicated openpuzzle-gpu-control executable.' >&2
    exit 1
fi
install -d -m 0755 /usr/libexec /usr/share/polkit-1/actions
install -o root -g root -m 0755 -- "$source_helper" /usr/libexec/openpuzzle-gpu-control
cat > /usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy <<'POLICY'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN" "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <vendor>OpenPuzzle</vendor>
  <action id="org.openpuzzle.gpu-control">
    <description>Change the selected GPU power limit or fan control</description>
    <message>Administrator authentication is required to change this GPU control</message>
    <defaults><allow_any>no</allow_any><allow_inactive>no</allow_inactive><allow_active>auth_admin</allow_active></defaults>
    <annotate key="org.freedesktop.policykit.exec.path">/usr/libexec/openpuzzle-gpu-control</annotate>
  </action>
</policyconfig>
POLICY
chown root:root /usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy
chmod 0644 /usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy
printf '%s\n' 'Dedicated GPU helper installed. No GPU setting changed.'
