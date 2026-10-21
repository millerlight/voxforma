#!/usr/bin/env bash
#
#  push-github.sh
#
#  Pusht das aktuelle Repo zu GitHub, ohne dass ein Passwort gespeichert wird.
#
#  Ablauf:
#    1. Rueckfrage nach dem GitHub-Benutzernamen
#    2. Rueckfrage nach dem Personal Access Token (Eingabe bleibt unsichtbar)
#    3. Push ueber eine temporaere URL, die nur fuer diesen einen Aufruf existiert
#
#  Der Token wird NICHT in .git/config geschrieben, NICHT in ~/.git-credentials
#  abgelegt und erscheint NICHT in der Shell-History, weil er ueber stdin
#  gelesen und per Environment-Variable uebergeben wird.
#
#  Verwendung:
#      ./push-github.sh                 # Branch "main" des aktuellen Repos
#      ./push-github.sh <branch>        # anderer Branch
#

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRANCH="${1:-main}"

cd "${REPO_DIR}"

# ---------------------------------------------------------------- Repository
if ! git rev-parse --git-dir >/dev/null 2>&1; then
    echo "FEHLER: ${REPO_DIR} ist kein Git-Repository." >&2
    exit 1
fi

if [ -z "$(git rev-parse --verify HEAD 2>/dev/null)" ]; then
    echo "FEHLER: Es gibt noch keinen Commit. Bitte zuerst committen." >&2
    exit 1
fi

# ------------------------------------------------------------------ Remote
REMOTE_URL="$(git remote get-url origin 2>/dev/null || true)"

if [ -z "${REMOTE_URL}" ]; then
    echo "FEHLER: Kein Remote 'origin' konfiguriert." >&2
    exit 1
fi

# Aus der Remote-URL die Teile holen. HTTPS und SSH werden unterstuetzt.
case "${REMOTE_URL}" in
    git@github.com:*)
        GITHUB_HOST="github.com"
        GITHUB_OWNER="$(printf '%s' "${REMOTE_URL}" | sed -E 's#^git@github\.com:([^/]+)/.*#\1#')"
        GITHUB_REPO="$(printf '%s' "${REMOTE_URL}" | sed -E 's#^git@github\.com:[^/]+/(.*)$#\1#')"
        ;;
    *)
        GITHUB_HOST="$(printf '%s' "${REMOTE_URL}" | sed -E 's#^[a-z]+://([^/]+)/.*#\1#')"
        GITHUB_OWNER="$(printf '%s' "${REMOTE_URL}" | sed -E 's#^[a-z]+://[^/]+/([^/]+)/.*#\1#')"
        GITHUB_REPO="$(printf '%s' "${REMOTE_URL}" | sed -E 's#^[a-z]+://[^/]+/[^/]+/(.*)$#\1#')"
        ;;
esac
GITHUB_REPO="${GITHUB_REPO%.git}"

if [ "${GITHUB_HOST}" != "github.com" ]; then
    echo "FEHLER: Das Skript ist nur fuer github.com gedacht (Remote ist ${GITHUB_HOST})." >&2
    exit 1
fi

# ------------------------------------------------------- Commit-Identitaet
# git committet mit Name und E-Mail. Beides wird hier abgefragt und nur fuer
# diesen einen Commit gesetzt, damit nichts dauerhaft in .git/config landet.
CURRENT_NAME="$(git config user.name || true)"
CURRENT_EMAIL="$(git config user.email || true)"

if [ -z "${CURRENT_NAME}" ] || [ -z "${CURRENT_EMAIL}" ]; then
    echo "=== Commit-Identitaet ==="
    echo "Git braucht Name und E-Mail, um den Commit zu signieren."
    echo "Die E-Mail muss eine bei GitHub hinterlegte Adresse sein, damit der"
    echo "Commit deinem Konto zugeordnet wird (siehe Hinweis unten)."
    echo
    printf "Name  [%s]: " "${CURRENT_NAME:-millerlight}"
    read -r NEW_NAME
    NEW_NAME="${NEW_NAME:-${CURRENT_NAME:-millerlight}}"

    printf "E-Mail (leer lassen fuer die GitHub-Noreply-Adresse): "
    read -r NEW_EMAIL
    if [ -z "${NEW_EMAIL}" ]; then
        NEW_EMAIL="${CURRENT_EMAIL:-millerlight@users.noreply.github.com}"
    fi
    echo

    export GIT_AUTHOR_NAME="${NEW_NAME}"
    export GIT_AUTHOR_EMAIL="${NEW_EMAIL}"
    export GIT_COMMITTER_NAME="${NEW_NAME}"
    export GIT_COMMITTER_EMAIL="${NEW_EMAIL}"

    # Existiert bereits ein Commit, wird sein Autor auf die neue Identitaet
    # gezogen, damit der Push korrekt attribuiert ist.
    if [ -n "$(git log -1 --format=%H 2>/dev/null)" ]; then
        git commit --amend --reset-author --no-edit -q
        echo "Autor des letzten Commits auf ${NEW_NAME} <${NEW_EMAIL}> gesetzt."
    fi
fi

# ------------------------------------------------------------- Anmeldedaten
echo "=== GitHub-Anmeldung ==="
echo "Repository: ${GITHUB_OWNER}/${GITHUB_REPO}"
echo "Branch:    ${BRANCH}"
echo
echo "Der Login-Benutzernamen ist dein GitHub-Benutzername."
echo "Als Passwort brauchst du einen Personal Access Token, KEIN Konto-Passwort."
echo "GitHub akzeptiert seit 2021 keine Passwort-Authentifizierung fuer Git."
echo "Token erzeugen: https://github.com/settings/tokens"
echo "  (Fine-grained token, Zugriff: Repository > Contents > Read and write)"
echo

printf "GitHub-Benutzername [%s]: " "${GITHUB_OWNER}"
read -r GITHUB_USER
GITHUB_USER="${GITHUB_USER:-${GITHUB_OWNER}}"

printf "Personal Access Token (Eingabe bleibt unsichtbar): "
read -rs GITHUB_TOKEN
echo
echo

if [ -z "${GITHUB_TOKEN}" ]; then
    echo "FEHLER: Kein Token eingegeben." >&2
    exit 1
fi

# --------------------------------------------------------------- Push
# Der Token geht per Environment ueber GIT_ASKPASS, damit er weder im
# Argument-Array (ps-Ausgabe) noch in der Shell-History landet.
ASKPASS_SCRIPT="$(mktemp)"
trap 'rm -f "${ASKPASS_SCRIPT}"' EXIT

cat > "${ASKPASS_SCRIPT}" <<'ASKPASS'
#!/usr/bin/env bash
case "$1" in
    *sername*) echo "${GITHUB_USER}" ;;
    *assword*) echo "${GITHUB_TOKEN}" ;;
esac
ASKPASS
chmod 700 "${ASKPASS_SCRIPT}"

export GITHUB_USER
export GITHUB_TOKEN
export GIT_ASKPASS="${ASKPASS_SCRIPT}"
export GIT_TERMINAL_PROMPT=0

echo "=== Push ==="
if git push --set-upstream "https://${GITHUB_HOST}/${GITHUB_OWNER}/${GITHUB_REPO}.git" "${BRANCH}:${BRANCH}"; then
    echo
    echo "Push erfolgreich."
    echo "Hinweis: Der Token wurde nicht gespeichert. Beim naechsten Push"
    echo "fragt Git erneut nach Benutzername und Token."
else
    echo
    echo "Push fehlgeschlagen. Moegliche Ursachen:"
    echo "  - Token ist abgelaufen oder hat zu wenig Rechte (Contents: Read and write)"
    echo "  - Repository existiert nicht oder du hast keinen Zugriff darauf"
    exit 1
fi
