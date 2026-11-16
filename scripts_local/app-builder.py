#!/usr/bin/env python3
"""A page in your browser for choosing the games and apps, building, and flashing.

    ./scripts_local/app-builder.py            # opens http://127.0.0.1:8765
    ./scripts_local/app-builder.py --port 9000 --no-browser

Tick what you want, press BUILD AND FLASH with the device on USB, and watch the
log. The choice is shared with pick-apps.py (crossplay-apps.local.txt).

Flashing writes the bootloader, partition table, blank otadata and app, exactly
as flash-sticky.sh does: NVS is left alone, and the new image is the one that
boots even if the device last took an over-the-air update.

Listens on 127.0.0.1 only.
"""

import argparse
import csv
import html
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("pick_apps", os.path.join(HERE, "pick-apps.py"))
pick = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pick)

REPO = pick.REPO
AUDIT = os.path.join(REPO, "docs", "audit", "app-memory-2026-11-08.csv")


def flash_costs():
    """KB of flash each title saves when left out, from the audit, when present."""
    costs = {}
    if not os.path.exists(AUDIT):
        return costs
    with open(AUDIT, encoding="utf-8") as source:
        for row in csv.DictReader(source):
            try:
                costs[row["app"]] = int(row["flash_saved_if_disabled_bytes"]) // 1024
            except (KeyError, ValueError):
                pass
    return costs


class Job:
    """The one build or flash running at a time, and its log."""

    def __init__(self):
        self.lock = threading.Lock()
        self.running = False
        self.log = []
        self.result = ""

    def start(self, work):
        with self.lock:
            if self.running:
                return False
            self.running = True
            self.log = []
            self.result = ""
        threading.Thread(target=self._run, args=(work,), daemon=True).start()
        return True

    def say(self, line):
        with self.lock:
            self.log.append(line.rstrip("\n"))
            del self.log[:-400]

    def _run(self, work):
        try:
            self.result = work()
        except Exception as error:  # surfaced on the page rather than lost in a thread
            self.result = f"failed: {error}"
        with self.lock:
            self.running = False

    def stream(self, command, environment=None):
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                   env=environment, bufsize=1)
        for line in process.stdout:
            self.say(line)
        return process.wait()


JOB = Job()


def espressif_ports():
    try:
        output = subprocess.run([pick.find_pio(), "device", "list", "--serial", "--json-output"],
                                capture_output=True, text=True, timeout=30).stdout
        return [port["port"] for port in json.loads(output) if "VID:PID=303A" in port.get("hwid", "").upper()]
    except (subprocess.SubprocessError, ValueError, SystemExit):
        return []


def build(kept, environment_name):
    items = pick.shelf_items()
    omitted = [macro for _, title, macro in items if title not in kept]
    with open(pick.CHOICE, "w", encoding="utf-8") as out:
        out.write("".join(title + "\n" for _, title, _ in items if title in kept))
    JOB.say(f"Building {environment_name} with {len(items) - len(omitted)} of {len(items)} games and apps.")
    environment = dict(os.environ)
    environment["PLATFORMIO_BUILD_FLAGS"] = " ".join("-D" + macro for macro in omitted)
    if JOB.stream([pick.find_pio(), "run", "-d", REPO, "-e", environment_name], environment) != 0:
        return "build failed"
    size = os.path.getsize(os.path.join(REPO, ".pio", "build", environment_name, "firmware.bin"))
    JOB.say(f"Built: firmware.bin is {size:,} bytes.")
    return "built"


def flash(environment_name, port):
    build_dir = os.path.join(REPO, ".pio", "build", environment_name)
    parts = [os.path.join(build_dir, name) for name in ("bootloader.bin", "partitions.bin", "firmware.bin")]
    if not all(os.path.exists(part) for part in parts):
        return "nothing built to flash"
    if not port:
        ports = espressif_ports()
        if len(ports) != 1:
            return ("no device found: plug the Sticky in with a data cable" if not ports
                    else f"{len(ports)} devices found ({', '.join(ports)}); choose one")
        port = ports[0]
    home = os.path.expanduser("~/.platformio")
    esptool = os.path.join(home, "packages", "tool-esptoolpy", "esptool.py")
    python = os.path.join(home, "penv", "Scripts" if os.name == "nt" else "bin",
                          "python.exe" if os.name == "nt" else "python")
    if not (os.path.exists(esptool) and os.path.exists(python)):
        return "esptool is missing; build once first"
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as blank:
        blank.write(b"\xff" * 0x2000)
    try:
        JOB.say(f"Flashing {environment_name} to {port}.")
        code = JOB.stream([python, esptool, "--chip", "esp32s3", "--port", port, "--baud", "921600",
                           "--before", "usb-reset", "--after", "hard-reset", "write-flash",
                           "0x0", parts[0], "0x8000", parts[1], "0xe000", blank.name, "0x10000", parts[2]])
    finally:
        os.remove(blank.name)
    if code != 0:
        return "flash failed: if it says no serial data was received, hold BOOT while tapping RESET and retry"
    return f"flashed {port}"


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>CrossPlay builder</title>
<style>
:root { --ink: #111; --paper: #fff; --line: #ccc; --muted: #666; }
@media (prefers-color-scheme: dark) { :root { --ink: #eee; --paper: #181818; --line: #444; --muted: #aaa; } }
body { background: var(--paper); color: var(--ink); font: 16px/1.4 system-ui, sans-serif; margin: 0 auto;
       max-width: 52rem; padding: 1rem; }
h1 { font-size: 1.4rem; margin: 0 0 .25rem; }
p.note { color: var(--muted); margin: 0 0 1rem; }
section { margin-bottom: 1rem; }
h2 { font-size: 1rem; margin: 0 0 .4rem; display: flex; gap: .75rem; align-items: baseline; }
h2 button { font-size: .8rem; }
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(13rem, 1fr)); gap: .25rem .75rem; }
label { display: flex; gap: .5rem; align-items: baseline; padding: .2rem 0; border-bottom: 1px solid var(--line); }
label small { color: var(--muted); margin-left: auto; }
.bar { position: sticky; bottom: 0; background: var(--paper); padding: .75rem 0; display: flex; flex-wrap: wrap;
       gap: .5rem; align-items: center; border-top: 1px solid var(--line); }
button { font: inherit; padding: .5rem .9rem; border: 2px solid var(--ink); background: var(--paper); color: var(--ink);
         cursor: pointer; }
button.primary { background: var(--ink); color: var(--paper); }
button:disabled { opacity: .5; cursor: default; }
select { font: inherit; padding: .4rem; }
#status { font-weight: 600; }
pre { background: #0001; padding: .75rem; height: 16rem; overflow: auto; white-space: pre-wrap; font-size: .8rem; }
</style></head><body>
<h1>CrossPlay builder</h1>
<p class="note">Tick what goes on the shelf. Everything else is left out of the firmware. Keep at least one game and
one app.</p>
__SECTIONS__
<div class="bar">
  <button class="primary" id="flashBuild">Build and flash</button>
  <button id="buildOnly">Build only</button>
  <select id="port" aria-label="Device"><option value="">Find the device</option></select>
  <span id="status"></span>
</div>
<pre id="log"></pre>
<script>
const statusEl = document.getElementById('status');
const logEl = document.getElementById('log');
const buttons = [document.getElementById('flashBuild'), document.getElementById('buildOnly')];
function kept() { return [...document.querySelectorAll('input[name=app]:checked')].map(b => b.value); }
document.querySelectorAll('[data-folder]').forEach(button => button.onclick = () => {
  const boxes = document.querySelectorAll(`input[data-group="${button.dataset.folder}"]`);
  const on = button.dataset.on === '1';
  boxes.forEach(b => b.checked = on);
});
async function ports() {
  const select = document.getElementById('port');
  const list = await (await fetch('/api/ports')).json();
  select.length = 1;
  list.forEach(p => select.add(new Option(p, p)));
}
async function start(withFlash) {
  const reply = await fetch('/api/run', {method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({keep: kept(), flash: withFlash, port: document.getElementById('port').value})});
  const body = await reply.json();
  if (!reply.ok) { statusEl.textContent = body.error; return; }
  poll();
}
async function poll() {
  const state = await (await fetch('/api/status')).json();
  logEl.textContent = state.log.join('\\n');
  logEl.scrollTop = logEl.scrollHeight;
  statusEl.textContent = state.running ? 'Working...' : state.result;
  buttons.forEach(b => b.disabled = state.running);
  if (state.running) setTimeout(poll, 1000);
}
buttons[0].onclick = () => start(true);
buttons[1].onclick = () => start(false);
ports();
poll();
</script></body></html>"""


def page():
    items = pick.shelf_items()
    kept = pick.saved_choice(items)
    costs = flash_costs()
    sections = []
    for folder in ("GAMES", "APPS"):
        boxes = []
        for item_folder, title, _ in items:
            if item_folder != folder:
                continue
            cost = f"<small>{costs[title]} KB</small>" if title in costs else ""
            checked = " checked" if title in kept else ""
            boxes.append(f'<label><input type="checkbox" name="app" data-group="{folder}" value="{html.escape(title)}"'
                         f'{checked}> {html.escape(title.title())}{cost}</label>')
        sections.append(f'<section><h2>{folder.title()} <button data-folder="{folder}" data-on="1">all</button>'
                        f'<button data-folder="{folder}" data-on="0">none</button></h2>'
                        f'<div class="grid">{"".join(boxes)}</div></section>')
    return PAGE.replace("__SECTIONS__", "\n".join(sections))


def make_handler(environment_name):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, code, body, kind="application/json"):
            data = body.encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", kind + "; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == "/":
                self.reply(200, page(), "text/html")
            elif self.path == "/api/status":
                with JOB.lock:
                    state = {"running": JOB.running, "result": JOB.result, "log": list(JOB.log)}
                self.reply(200, json.dumps(state))
            elif self.path == "/api/ports":
                self.reply(200, json.dumps(espressif_ports()))
            else:
                self.reply(404, json.dumps({"error": "not found"}))

        def do_POST(self):
            if self.path != "/api/run":
                self.reply(404, json.dumps({"error": "not found"}))
                return
            try:
                request = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))) or b"{}")
            except ValueError:
                self.reply(400, json.dumps({"error": "bad request"}))
                return
            items = pick.shelf_items()
            titles = {title for _, title, _ in items}
            kept = {title for title in request.get("keep", []) if title in titles}
            for folder in ("GAMES", "APPS"):
                if not any(f == folder and title in kept for f, title, _ in items):
                    self.reply(400, json.dumps({"error": f"Keep at least one of the {folder.lower()}."}))
                    return
            port = str(request.get("port", ""))
            with_flash = bool(request.get("flash"))

            def work():
                result = build(kept, environment_name)
                if result != "built" or not with_flash:
                    return result
                return flash(environment_name, port)

            if not JOB.start(work):
                self.reply(409, json.dumps({"error": "Already working."}))
                return
            self.reply(202, json.dumps({"started": True}))

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8765, help="local port for the page")
    parser.add_argument("--env", default="gh_release_sticky", help="PlatformIO environment")
    parser.add_argument("--no-browser", action="store_true", help="do not open a browser")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(args.env))
    address = f"http://127.0.0.1:{args.port}/"
    print(f"CrossPlay builder at {address}  (Ctrl+C to stop)")
    if not args.no_browser:
        webbrowser.open(address)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    sys.exit(main())
