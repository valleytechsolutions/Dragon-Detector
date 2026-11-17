"""Upload a built Dragon Detector web flasher site (.build/site) to Cloudflare Pages.

Two steps, both run from the repository root with PlatformIO's Python
("$env:USERPROFILE\\.platformio\\penv\\Scripts\\python.exe" on the owner's PC):

  python tools/deploy_web_flasher.py prepare
      Run by whoever built and checked the site. Reads the live site and writes
      .build/deploy-plan.json: every site file with its SHA-256, the live files
      the upload replaces (as they are now) and the files to read back after it.

  python tools/deploy_web_flasher.py upload [--sign-in-again]
      Run by the owner (production). It stops at the first problem and says why:
       1. checks .build/site is exactly the prepared site, file for file
       2. checks the live site is still what the plan was prepared against
          (if this upload is already live, it skips to step 7)
       3. checks Node.js 22 or newer
       4. gets Wrangler and signs in to Cloudflare in the browser if needed
       5. finds the Cloudflare account that owns the valleytech-dragon-detector project
       6. uploads to the production branch (main) and checks it became production
       7. reads the planned files back from the live site
      Everything also goes to .build/deploy-log.txt, with e-mail addresses left out.
      The last line starts with DEPLOY OK, DEPLOY UPLOADED or DEPLOY STOPPED.

Wrangler runs in an empty temporary folder with its own cache and without any
CLOUDFLARE_*, CF_* or WRANGLER_* setting from this PC, so no wrangler.toml,
cached account or API token can send the upload elsewhere.
"""
import argparse
import atexit
import datetime
import hashlib
import importlib
import json
import os
import re
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, ".build")
SITE = os.path.join(BUILD, "site")
PLAN = os.path.join(BUILD, "deploy-plan.json")
LOG = os.path.join(BUILD, "deploy-log.txt")
PROJECT = "valleytech-dragon-detector"
PAGES_DOMAIN = PROJECT + ".pages.dev"
SITE_URL = "https://" + PAGES_DOMAIN + "/"
WRANGLER = "wrangler@4.139.0"
WRANGLER_VERSION = WRANGLER.split("@")[1]
# npx keeps its copy of wrangler@4.139.0 in <npm's cache folder>/_npx/<the first 16 hex digits of this>.
NPX_FOLDER = hashlib.sha512(WRANGLER.encode("utf-8")).hexdigest()[:16]
NODE_MAJOR = 22
# Site paths ("" is the front page) whose live copy tells which version is live.
WATCHED = ("", "app.js", "board-profiles.js")
EMAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
AGAIN = "tools/deploy_web_flasher.py upload --sign-in-again"
WRONG_ACCOUNT = ("this Cloudflare sign-in has no project called %s (%s). Sign in with the account that hosts the "
                 "flasher: if the Cloudflare page in your browser shows another account, log out of it there first. "
                 "Then run %s and choose Allow." % (PROJECT, PAGES_DOMAIN, AGAIN))
# Settings in this PC's environment that would change Wrangler's account, sign-in, API or cache: not passed on.
INHERITED = ("CLOUDFLARE_", "CF_", "WRANGLER_")
OURS = ("dragon-detector-deploy-", "dragon-detector-wrangler-")
# Node.js error codes for a secure connection whose certificate it does not trust.
CERTIFICATE_ERRORS = ("UNABLE_TO_GET_ISSUER_CERT", "SELF_SIGNED_CERT_IN_CHAIN", "DEPTH_ZERO_SELF_SIGNED_CERT",
                      "UNABLE_TO_VERIFY_LEAF_SIGNATURE", "CERT_UNTRUSTED", "CERT_HAS_EXPIRED")


class Log:
    def __init__(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self.handle = open(path, "a", encoding="utf-8")

    def __call__(self, text=""):
        for line in str(text).splitlines() or [""]:
            line = EMAIL.sub("(e-mail left out)", line)
            stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            self.handle.write("%s | %s\n" % (stamp, line))
            try:
                print(line, flush=True)
            except UnicodeEncodeError:
                print(line.encode("ascii", "replace").decode("ascii"), flush=True)
        self.handle.flush()


def stop(log, reason, retry=False):
    log("")
    log("DEPLOY STOPPED: " + reason)
    if retry:
        log("If it stops here again, pass on the log: %s" % LOG)
    sys.exit(1)


def describe(code):
    return "no answer within the time limit" if code is None else "exit code %d" % code


def temporary_folder(prefix):
    path = tempfile.mkdtemp(prefix=prefix)
    atexit.register(shutil.rmtree, path, True)
    return path


def remove_leftovers(log):
    """Closing the window skips the clean-up at the end, so remove this script's temporary folders from earlier
    runs (more than six hours old, so a run still going in another window keeps its folders)."""
    base = tempfile.gettempdir()
    try:
        names = os.listdir(base)
    except OSError:
        return
    for name in names:
        path = os.path.join(base, name)
        try:
            old = time.time() - os.path.getmtime(path) > 6 * 3600
        except OSError:
            continue
        if name.startswith(OURS) and old and os.path.isdir(path) and not os.path.islink(path):
            shutil.rmtree(path, ignore_errors=True)
            log("   Removed %s, left over from an earlier run." % path)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def site_files(folder):
    """{site path: [bytes, SHA-256]} for every file under folder."""
    found = {}
    for root, _, names in os.walk(folder):
        for name in names:
            full = os.path.join(root, name)
            relative = os.path.relpath(full, folder).replace(os.sep, "/")
            found[relative] = [os.path.getsize(full), sha256_file(full)]
    return found


def site_path(path):
    """The file a site path is served from: "" is index.html."""
    return path or "index.html"


# ---------------------------------------------------------------- reading the live site

CONTEXTS = []


def tls_contexts():
    """The system's certificates first; then Mozilla's list from certifi (PlatformIO's Python carries it),
    for a PC whose certificate store lacks the site's root certificate."""
    if not CONTEXTS:
        CONTEXTS.append(ssl.create_default_context())
        for module in ("certifi", "pip._vendor.certifi"):
            try:
                CONTEXTS.append(ssl.create_default_context(cafile=importlib.import_module(module).where()))
                break
            except Exception:
                continue
    return CONTEXTS


def certificate_problem(error):
    return isinstance(error, ssl.SSLCertVerificationError) or isinstance(getattr(error, "reason", None), ssl.SSLCertVerificationError)


def fetch_hash(base, path):
    """SHA-256 of the live file, or None if the site answers 404."""
    url = base + path + "?check=%d" % time.time_ns()
    request = urllib.request.Request(url, headers={"Cache-Control": "no-cache", "User-Agent": "dragon-detector-deploy-check"})
    contexts = tls_contexts()
    for number, context in enumerate(contexts):
        try:
            with urllib.request.urlopen(request, timeout=30, context=context) as response:
                return hashlib.sha256(response.read()).hexdigest()
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return None
            raise
        except (urllib.error.URLError, ssl.SSLError) as error:
            if number == len(contexts) - 1 or not certificate_problem(error):
                raise


def fetch_all(log, base, paths):
    seen = {}
    for attempt in range(3):
        try:
            for path in paths:
                if path not in seen:
                    seen[path] = fetch_hash(base, path)
            return seen
        except Exception as error:
            if attempt == 2:
                stop(log, "could not read the live site (%s). Check the internet connection and run this again." % error,
                     retry=True)
            time.sleep(5)


# ---------------------------------------------------------------- prepare

def version():
    with open(os.path.join(ROOT, "firmware", "include", "dragon_version.h"), encoding="utf-8") as handle:
        return re.search(r'#define DRAGON_VERSION "([0-9.]+)"', handle.read()).group(1)


def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def prepare(args):
    log = Log(LOG)
    log("=== Preparing the web flasher upload, %s ===" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
    if not os.path.isfile(os.path.join(SITE, "index.html")):
        stop(log, "%s has no index.html. Build it first: python tools/build_static_site.py --output .build/site" % SITE)
    files = site_files(SITE)
    boards = sorted(path.split("/")[1] for path in files if re.fullmatch(r"firmware/[^/]+/factory\.json", path))
    checks = [path for path in WATCHED if site_path(path) in files]
    checks += ["firmware/%s/%s" % (board, name) for board in boards for name in ("factory.json", "update.json")]
    log("Reading %d files from the live site ..." % len(checks))
    live = fetch_all(log, args.site_url, checks)
    after = {path: files[site_path(path)][1] for path in checks}
    changed = [path or "(front page)" for path in checks if live[path] != after[path]]
    if not changed:
        # The same version again (a rebuilt image, a corrected guide): compare every file.
        log("Those match the live site; comparing all %d files ..." % len(files))
        everything = fetch_all(log, args.site_url, [path for path in sorted(files) if path != "index.html"])
        differ = [path for path in sorted(everything) if everything[path] != files[path][1]]
        if not differ:
            stop(log, "the live site already serves this build; there is nothing to upload.")
        changed = differ
        for path in differ[:40]:
            checks.append(path)
            after[path] = files[path][1]
    plan = {
        "version": version(),
        "commit": git("rev-parse", "HEAD"),
        "dirty": bool(git("status", "--porcelain", "--untracked-files=no")),
        "prepared": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "site_url": args.site_url,
        "files": files,
        "live_before": {path: live[path] for path in WATCHED if path in live},
        "checks": after,
    }
    with open(PLAN, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(plan, handle, indent=1, sort_keys=True)
        handle.write("\n")
    log("Wrote %s: version %s, %d files, %d bytes." % (PLAN, plan["version"], len(files), sum(size for size, _ in files.values())))
    log("These planned read-back files differ from the live site: %s" % ", ".join(changed))
    log("PREPARED: the owner can now run  python tools/deploy_web_flasher.py upload")


# ---------------------------------------------------------------- running Node and Wrangler

TOOLS = {}


def tool(name):
    os.environ["NoDefaultCurrentDirectoryInExePath"] = "1"  # never pick a node.exe from the current folder
    found = shutil.which(name)
    if found is None and os.name == "nt":
        for base in (os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", ""), os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs")):
            candidate = os.path.join(base, "nodejs", name + (".cmd" if name == "npx" else ".exe"))
            if base and os.path.isfile(candidate):
                return candidate
    return found


def end(process):
    if os.name == "nt":  # npx is a .cmd script: end node as well, not only cmd.exe
        subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        process.kill()


def windows_certificates():
    """This PC's trusted certificates as a file for Node.js, or None. Node trusts only its own list, and an
    antivirus that scans secure connections adds its own root certificate to Windows' list instead."""
    if os.name != "nt" or not hasattr(ssl, "enum_certificates"):
        return None
    certificates = set()
    for store in ("CA", "ROOT"):  # the stores, and the purpose check, that Python itself trusts
        try:
            entries = ssl.enum_certificates(store)
        except OSError:
            continue
        for der, encoding, trust in entries:
            if encoding != "x509_asn" or not (trust is True or ssl.Purpose.SERVER_AUTH.oid in trust):
                continue
            try:
                ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT).load_verify_locations(cadata=der)  # skip one OpenSSL rejects
            except (ssl.SSLError, ValueError):
                continue
            certificates.add(ssl.DER_cert_to_PEM_cert(der))
    if not certificates:
        return None
    path = os.path.join(temporary_folder("dragon-detector-wrangler-certificates-"), "windows-certificates.pem")
    with open(path, "w", encoding="ascii", newline="\n") as handle:
        handle.write("".join(sorted(certificates)))
    return path


def isolate(log):
    """Run Wrangler in an empty folder with a cache of its own, so that no wrangler.toml, functions folder or
    cached account on this PC counts, and with none of this PC's Cloudflare or Wrangler settings."""
    TOOLS["work"] = temporary_folder("dragon-detector-deploy-")
    TOOLS["environment"] = {"WRANGLER_SEND_METRICS": "false",
                            "WRANGLER_CACHE_DIR": temporary_folder("dragon-detector-wrangler-cache-")}
    folder = TOOLS["work"]
    while True:  # Wrangler also reads a settings file in any folder above the one it runs in
        for name in ("wrangler.json", "wrangler.jsonc", "wrangler.toml", os.path.join(".wrangler", "deploy", "config.json")):
            path = os.path.join(folder, name)
            if os.path.isfile(path):
                stop(log, "the Wrangler settings file %s would change where this upload goes. Rename it (for example "
                          "to %s.off), run this again, and rename it back afterwards." % (path, os.path.basename(path)))
        parent = os.path.dirname(folder)
        if parent == folder:
            break
        folder = parent
    ignored = sorted(name for name in os.environ if name.upper().startswith(INHERITED))
    if ignored:
        log("   Not passing these settings from this PC to Wrangler: %s." % ", ".join(ignored))
    certificates = None if os.environ.get("NODE_EXTRA_CA_CERTS") else windows_certificates()
    if certificates:
        TOOLS["environment"]["NODE_EXTRA_CA_CERTS"] = certificates
    proxy = urllib.request.getproxies().get("https")  # on Windows also the proxy set in Windows' settings
    if proxy and not (os.environ.get("HTTPS_PROXY") or os.environ.get("https_proxy")):
        TOOLS["environment"]["HTTPS_PROXY"] = proxy
        log("   Using this PC's proxy setting for Node.js too.")


def run(log, args, capture=True, timeout=None, account=None, extra=None):
    """Run a command and return (exit code, or None if it ran out of time, output).
    With capture=False it shares this window."""
    if "work" not in TOOLS:
        isolate(log)
    environment = {name: value for name, value in os.environ.items() if not name.upper().startswith(INHERITED)}
    environment.update(TOOLS["environment"])
    folders = [TOOLS[key] for key in ("node_dir", "npx_dir") if TOOLS.get(key)]
    if folders:  # Wrangler's own launcher runs `node` from PATH
        environment["PATH"] = os.pathsep.join(folders + [environment.get("PATH", "")])
    if account:
        environment["CLOUDFLARE_ACCOUNT_ID"] = account
    environment.update(extra or {})
    log("   > " + " ".join([os.path.basename(args[0])] + list(args[1:])))
    options = {"env": environment, "cwd": TOOLS["work"]}
    if capture:
        options.update(stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    process = subprocess.Popen(args, **options)
    expired = threading.Event()

    def expire():
        expired.set()
        end(process)

    timer = threading.Timer(timeout, expire) if timeout else None
    if timer:
        timer.daemon = True
        timer.start()
    lines = []
    try:
        if capture:
            for raw in process.stdout:
                line = ANSI.sub("", raw.decode("utf-8", "replace").rstrip("\r\n"))
                lines.append(line)
                log("     " + line)
        process.wait()
    finally:
        if timer:
            timer.cancel()
    if expired.is_set():
        log("   It gave no answer within %d minutes, so it was stopped." % max(1, timeout // 60))
        return None, "\n".join(lines)
    return process.returncode, "\n".join(lines)


def check_node(log):
    log("3. Checking Node.js ...")
    isolate(log)
    node, npx = tool("node"), tool("npx")
    if not node or not npx:
        stop(log, "Node.js is not installed. Install the LTS version from https://nodejs.org/ "
                  "(keep the default options), open a new PowerShell window and run this again.")
    TOOLS["node_dir"], TOOLS["npx_dir"] = os.path.dirname(node), os.path.dirname(npx)
    code, output = run(log, [node, "--version"], timeout=120)
    match = re.search(r"v(\d+)\.", output)
    if code != 0 or not match:
        stop(log, "could not read the Node.js version (%s)." % describe(code))
    if int(match.group(1)) < NODE_MAJOR:
        stop(log, "Node.js %s is too old for Wrangler. Install the LTS version from https://nodejs.org/, "
                  "open a new PowerShell window and run this again." % output.strip())
    return npx


def npx_copy(log, npx):
    """The folder in npm's cache that holds npx's copy of wrangler@4.139.0."""
    npm = os.path.join(os.path.dirname(npx), "npm.cmd" if os.name == "nt" else "npm")
    cache = None
    if os.path.isfile(npm):
        code, output = run(log, [npm, "config", "get", "cache"], timeout=300)
        lines = [line.strip() for line in output.splitlines() if line.strip()]
        if code == 0 and lines and os.path.isabs(lines[-1]):
            cache = lines[-1]
    if cache is None:  # npm's default
        cache = os.path.join(os.environ.get("LOCALAPPDATA", ""), "npm-cache") if os.name == "nt" else os.path.expanduser("~/.npm")
    return os.path.join(cache, "_npx", NPX_FOLDER)


def get_wrangler(log, npx):
    """npx downloads Wrangler the first time. A download that was cut off (a closed window, a lost connection)
    leaves a copy that npx counts as installed but that cannot start, and every later run would fail on it,
    so such a copy is removed and downloaded once more."""
    log("   Getting Wrangler %s. The first time this downloads about 70 MB, which can take several minutes; "
        "leave this window open." % WRANGLER_VERSION)
    for attempt in (1, 2):
        code, output = run(log, [npx, "--yes", WRANGLER, "--version"], timeout=2700)
        if re.search(r"(?m)^\s*%s\s*$" % re.escape(WRANGLER_VERSION), output):
            return
        folder = npx_copy(log, npx)
        if os.path.isdir(folder):
            log("   Removing the copy of Wrangler that did not start (%s)." % folder)
            shutil.rmtree(folder, ignore_errors=True)
        if code is None:  # it ran out of time: trying again would take as long
            break
    if any(marker in output for marker in CERTIFICATE_ERRORS):
        stop(log, "Wrangler could not be downloaded because something on this PC intercepts secure connections "
                  "(usually an antivirus that scans HTTPS traffic, or a proxy).")
    stop(log, "Wrangler could not be downloaded or started (%s). Check the internet connection and run this again."
         % describe(code), retry=True)


def whoami(log, npx):
    code, output = run(log, [npx, "--yes", WRANGLER, "whoami"], timeout=600)
    if "Getting User settings" not in output:  # Wrangler itself never started
        stop(log, "Wrangler could not be started (%s). Check the internet connection and run this again."
             % describe(code), retry=True)
    return code == 0 and "you are logged in" in output.lower() and "not authenticated" not in output.lower()


def sign_in(log, npx, again):
    log("4. Getting Wrangler and checking the Cloudflare sign-in ...")
    get_wrangler(log, npx)
    if again:
        log("   Signing out first, as asked.")
        run(log, [npx, "--yes", WRANGLER, "logout"], timeout=600)
    if whoami(log, npx):
        log("   Wrangler is signed in.")
        return
    output = ""
    for attempt in (1, 2):
        if attempt == 1:
            log("   Not signed in. A browser window opens: within two minutes, sign in to Cloudflare with the "
                "account that hosts the flasher and choose Allow.")
        else:
            log("   The sign-in did not finish, so the browser opens once more: sign in within two minutes and "
                "choose Allow.")
        _, output = run(log, [npx, "--yes", WRANGLER, "login"], timeout=900)  # its messages go to the log too
        if whoami(log, npx):
            log("   Wrangler is signed in.")
            return
    late = " (the browser sign-in took longer than two minutes)" if "Timed out waiting for authorization" in output else ""
    stop(log, "Wrangler is still not signed in to Cloudflare%s. Run this again and finish the browser sign-in "
              "within two minutes." % late, retry=True)


def projects_in(output):
    """The rows of `wrangler pages project list --json`, or None if the output holds no such list."""
    lines = output.splitlines()
    for first, line in enumerate(lines):
        if not line.strip().startswith("["):
            continue
        for last in range(len(lines) - 1, first - 1, -1):
            if not lines[last].strip().endswith("]"):
                continue
            try:
                rows = json.loads("\n".join(lines[first:last + 1]))
            except ValueError:
                continue
            if isinstance(rows, list):
                return rows
    return None


def owns_project(rows):
    for row in rows or []:
        if isinstance(row, dict) and row.get("Project Name") == PROJECT:
            if PAGES_DOMAIN in [domain.strip() for domain in str(row.get("Project Domains", "")).split(",")]:
                return True
    return False


def find_account(log, npx):
    """Return None when Wrangler picks the right account by itself, else the ID of the account to use."""
    log("5. Looking for the %s project ..." % PROJECT)
    command = [npx, "--yes", WRANGLER, "pages", "project", "list", "--json"]
    code, output = run(log, command, timeout=600)
    if code == 0:
        rows = projects_in(output)
        if rows is None:
            stop(log, "could not read Wrangler's list of Pages projects.")
        if not owns_project(rows):
            stop(log, WRONG_ACCOUNT)
        log("   Found it.")
        return None
    accounts = sorted(set(re.findall(r"`([0-9a-f]{32})`", output)))
    if "More than one account" not in output or not accounts:
        stop(log, "Wrangler could not list the Pages projects (%s)." % describe(code))
    log("   This sign-in has %d Cloudflare accounts; checking each one." % len(accounts))
    owners = []
    for account in accounts:
        code, output = run(log, command, timeout=600, account=account)
        if code == 0 and owns_project(projects_in(output)):
            owners.append(account)
    if not owners:
        stop(log, WRONG_ACCOUNT)
    if len(owners) > 1:
        stop(log, "%d accounts have the %s project." % (len(owners), PROJECT))
    log("   Found it in account %s." % owners[0])
    return owners[0]


# ---------------------------------------------------------------- upload

def load_plan(log):
    try:
        with open(PLAN, encoding="utf-8") as handle:
            plan = json.load(handle)
    except (OSError, ValueError) as error:
        stop(log, "no usable upload plan at %s (%s). The person who built the site runs "
                  "python tools/deploy_web_flasher.py prepare first." % (PLAN, error))
    for key in ("version", "commit", "files", "live_before", "checks", "site_url"):
        if key not in plan:
            stop(log, "the upload plan %s has no %r." % (PLAN, key))
    return plan


def verify_site(log, plan):
    log("1. Checking %s against the prepared site ..." % SITE)
    found = site_files(SITE) if os.path.isdir(SITE) else {}
    expected = plan["files"]
    if found != expected:
        extra = sorted(set(found) - set(expected))[:3]
        missing = sorted(set(expected) - set(found))[:3]
        changed = sorted(path for path in set(found) & set(expected) if found[path] != expected[path])[:3]
        stop(log, "the site folder is not the prepared one (added %s, missing %s, changed %s). Rebuild and "
                  "prepare again; nothing was uploaded." % (extra, missing, changed))
    log("   %d files, %d bytes, version %s, commit %s%s." % (len(found), sum(size for size, _ in found.values()),
        plan["version"], plan["commit"][:12] or "unknown", " (with uncommitted changes)" if plan.get("dirty") else ""))


def live_state(log, plan, base):
    """Return "before" if the live site is what the plan replaces, "after" if this upload is live already."""
    log("2. Checking the live site at %s ..." % base)
    watched = sorted(plan["live_before"])
    seen = fetch_all(log, base, watched)
    if all(seen[path] == plan["live_before"][path] for path in watched):
        log("   It is the version this upload was prepared against.")
        return "before"
    if all(seen[path] == plan["checks"].get(path) for path in watched):
        log("   This upload is already live, so there is nothing to upload.")
        return "after"
    changed = [path or "the front page" for path in watched if seen[path] not in (plan["live_before"][path], plan["checks"].get(path))]
    stop(log, "the live site changed after this upload was prepared (%s). Uploading now would replace that newer "
              "version, so nothing was uploaded. Prepare again." % (", ".join(changed) or "it is partly old and partly new"))


def deploy(log, npx, plan, account):
    log("6. Uploading to %s (production branch main) ..." % PROJECT)
    report = os.path.join(temporary_folder("dragon-detector-deploy-output-"), "wrangler-output.json")
    # Each option next to its value: Wrangler parses strictly, so a value out of place stops the upload.
    options = [("--project-name", PROJECT), ("--branch", "main"), ("--commit-message", "Dragon Detector %s" % plan["version"])]
    if re.fullmatch(r"[0-9a-f]{40}", plan["commit"] or ""):
        options.append(("--commit-hash", plan["commit"]))
    command = [npx, "--yes", WRANGLER, "pages", "deploy", SITE]
    for option, value in options:
        command += [option, value]
    command.append("--commit-dirty=true")
    code, output = run(log, command, timeout=3600, account=account, extra={"WRANGLER_OUTPUT_FILE_PATH": report})
    if code != 0:
        stop(log, "the upload did not finish (%s)." % describe(code))
    details = None
    try:
        with open(report, encoding="utf-8") as handle:
            for line in handle:
                try:
                    entry = json.loads(line)
                except ValueError:
                    continue
                if isinstance(entry, dict) and entry.get("type") == "pages-deploy-detailed":
                    details = entry
    except OSError as error:
        log("   Wrangler's deployment report could not be read (%s); the read-back below still checks the live site." % error)
    if details is not None:
        if details.get("pages_project") != PROJECT or details.get("environment") != "production":
            stop(log, "the upload became a %s deployment of %s, not the live site (the project's production branch "
                      "is %s, not main)." % (details.get("environment"), details.get("pages_project"),
                                            details.get("production_branch")))
        log("   Upload finished: production deployment %s." % details.get("url"))
    else:
        log("   Upload finished.")


def read_back(log, plan, base):
    log("7. Reading files back from %s ..." % base)
    pending = dict(plan["checks"])
    for attempt in range(12):
        for path, expected in list(pending.items()):
            try:
                if fetch_hash(base, path) == expected:
                    log("   ok   %s" % (path or "(front page)"))
                    del pending[path]
            except Exception as error:
                if attempt == 11:
                    log("   %s: %s" % (path or "(front page)", error))
        if not pending:
            return True
        time.sleep(10)
    for path in pending:
        log("   NOT YET %s" % (path or "(front page)"))
    return False


def upload(args):
    log = Log(LOG)
    log("=== Dragon Detector web flasher upload, %s ===" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
    log("Python %s on %s" % (sys.version.split()[0], sys.platform))
    plan = load_plan(log)
    base = args.site_url or plan["site_url"]
    try:
        remove_leftovers(log)
        verify_site(log, plan)
        if live_state(log, plan, base) == "before":
            npx = check_node(log)
            sign_in(log, npx, args.sign_in_again)
            account = find_account(log, npx)
            verify_site(log, plan)  # once more, right before the upload
            deploy(log, npx, plan, account)
        if read_back(log, plan, base):
            log("")
            log("DEPLOY OK: the web flasher now serves Dragon Detector %s." % plan["version"])
        else:
            log("")
            log("DEPLOY UPLOADED, but the live site has not shown every new file yet. Check again in a few minutes.")
    except KeyboardInterrupt:
        log("")
        log("DEPLOY STOPPED: interrupted from the keyboard. Check what is live before running it again.")
        sys.exit(130)
    except SystemExit:
        raise
    except Exception:
        log(traceback.format_exc())
        stop(log, "the upload script hit an unexpected error (shown above).")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="step", required=True)
    first = sub.add_parser("prepare", help="record the built site and the live site's current state")
    first.add_argument("--site-url", default=SITE_URL, help=argparse.SUPPRESS)
    second = sub.add_parser("upload", help="upload the prepared site (production)")
    second.add_argument("--sign-in-again", action="store_true", help="sign out of Cloudflare first")
    second.add_argument("--site-url", default=None, help=argparse.SUPPRESS)
    args = parser.parse_args()
    prepare(args) if args.step == "prepare" else upload(args)


if __name__ == "__main__":
    main()
