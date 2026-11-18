#!/usr/bin/env python3
"""Size all 30 Almond memalloc entries before startup (Python 3.6+, Linux).

Examples (the preview ALSO RUNS the configured monitoring commands):
  python3 almond_memalloc_autosize.py --samples 3
  python3 almond_memalloc_autosize.py --samples 3 --apply --enable
  python3 almond_memalloc_autosize.py --static-only --plugins-conf ./plugins.conf

Run as the Almond service user, with the same environment and working directory.
Reads /etc/almond/almond.conf for configured paths, hostname and file names;
--almond-conf selects another file. Plugin paths come from that configuration
unless explicitly overridden. Other defaults are /opt/almond/memalloc.alm and
/etc/almond/memalloc.conf. Missing config-dependent inputs retain existing sizes.
Only run this against trusted plugin declarations: commands use /bin/sh just
as Almond does. No shell command text or plugin output is printed or retained.

Every entry is reported with its sizing basis: measured, derived, modelled, or
retained. Paths cover initial defaults, configured values (including duplicate
assignments), and composite log/data/backup/CSV/archive names. API and logging
sizes use conservative models of the supplied C formats, plugin samples and
configured strings; these are estimates, not proof of all future requirements.
Static sizes cover ALL declarations, including inactive ones. Output is sampled
from active=1 declarations unless --include-inactive is set. All stdout bytes
are counted, including line endings; stderr is drained but is not Almond data.
Exit codes 0..3 are valid monitoring results. A failed, empty, timed-out, binary,
or oversized sample prevents ALL changes. --static-only cannot write anything.

For names, descriptions and commands, the stored value is the measured byte
length (at least 1). Output uses max(64, ceil(observed * (1 + margin/100)) +
output_reserve), with 20% and 64 spare bytes by default. Almond getConstants()
already adds one byte; this script does not add another NUL byte to its values.
No sample can establish a future worst case: use representative load, more
samples, larger margins, or --grow-only to retain existing larger values.

Future inbound HTTP requests/JWT headers and gardener output cannot be inferred
from plugins.conf. SOCKETCLIENTMESSAGE_SIZE, SOCKETSERVERMESSAGE_SIZE, and
GARDENERMESSAGE_SIZE retain their current values without representative captures.
The supplied server_message allocation is not populated in the examined code;
it is separate from dynamically allocated socket_message responses.
To measure those buffers too, supply --runtime-samples captures.json containing
a mapping from a runtime key to a nonempty list of sample-file paths, e.g.:
  {"SOCKETCLIENTMESSAGE_SIZE": ["request.http"],
   "GARDENERMESSAGE_SIZE": ["gardener.stdout"]}
Files are relative to the manifest. HTTP captures must contain complete headers
and bodies, not just JSON. LOGMESSAGE_SIZE and INFOSTR_SIZE captures are measured
by longest line; APIMESSAGE_SIZE, socket buffers and gardener stdout by file size.
Gardener commands are never executed automatically because they may remove data.

Unknown memalloc entries are preserved, with backups and atomic replacement on
--apply. --enable additionally writes 'enabled' to memalloc.conf, with backup.
Restart Almond afterwards; existing allocations cannot be resized this way.

Install the updated main.c and the earlier dynamic-reader plugins.c and rebuild
before applying this expanded optimization. main.c fixes bootstrap logging,
inventory allocation, fixed 50/100/150-byte writes, and truncated config values;
shrinking the affected allocations in the original C code would be unsafe.
The supplied run_plugin() stores the last non-empty output line; a larger
buffer does not make Almond combine multi-line plugin output.
"""

import argparse
import datetime
import json
import math
import os
from pathlib import Path
import re
import selectors
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import time
from collections import namedtuple


Plugin = namedtuple("Plugin", "name description command active line")
PLUGIN_KEYS = ("PLUGINITEMNAME_SIZE", "PLUGINITEMDESC_SIZE", "PLUGINITEMCMD_SIZE",
               "PLUGINCOMMAND_SIZE", "PLUGINOUTPUT_SIZE", "PLUGINMESSAGE_SIZE")
DEFAULTS = {
    "PLUGINITEMNAME_SIZE": 50, "PLUGINITEMDESC_SIZE": 100, "PLUGINITEMCMD_SIZE": 255,
    "PLUGINOUTPUT_SIZE": 1800, "CONFDIR_SIZE": 50, "DATADIR_SIZE": 50,
    "STOREDIR_SIZE": 50, "LOGDIR_SIZE": 50, "PLUGINDIR_SIZE": 50,
    "PLUGINDECLARATIONFILE_SIZE": 75, "FILENAME_SIZE": 150, "HOSTNAME_SIZE": 255,
    "JSONFILENAME_SIZE": 50, "METRICSFILENAME_SIZE": 50, "INVENTORYFILENAME_SIZE": 125,
    "GARDENERSCRIPT_SIZE": 75, "METRICSOUTPUTPREFIX_SIZE": 30,
    "LOGMESSAGE_SIZE": 1545, "INFOSTR_SIZE": 400, "LOGFILE_SIZE": 100,
    "DATAFILENAME_SIZE": 100, "BACKUPDIRECTORY_SIZE": 100, "NEWFILENAME_SIZE": 250,
    "GARDENERMESSAGE_SIZE": 1035, "PLUGINCOMMAND_SIZE": 250, "PLUGINMESSAGE_SIZE": 2000,
    "STORENAME_SIZE": 100, "APIMESSAGE_SIZE": 2000,
    "SOCKETSERVERMESSAGE_SIZE": 2000, "SOCKETCLIENTMESSAGE_SIZE": 8192,
}
KEYS = tuple(DEFAULTS)
RUNTIME_KEYS = {"LOGMESSAGE_SIZE", "INFOSTR_SIZE", "APIMESSAGE_SIZE",
                "SOCKETCLIENTMESSAGE_SIZE", "SOCKETSERVERMESSAGE_SIZE", "GARDENERMESSAGE_SIZE"}
PATH_FIELDS = {
    "CONFDIR_SIZE": ("scheduler.confDir", None),
    "DATADIR_SIZE": ("scheduler.dataDir", None),
    "STOREDIR_SIZE": ("scheduler.storeDir", None),
    "LOGDIR_SIZE": ("scheduler.logDir", None),
    "JSONFILENAME_SIZE": ("data.jsonFile", "monitor_data.json"),
    "METRICSFILENAME_SIZE": ("data.metricsFile", "monitor.metrics"),
    "INVENTORYFILENAME_SIZE": ("almond.inventoryFileName", "/opt/almond/data/inventory.json"),
    "GARDENERSCRIPT_SIZE": ("scheduler.gardenerScript", "/opt/almond/gardener.py"),
    "METRICSOUTPUTPREFIX_SIZE": ("data.metricsOutputPrefix", "almond"),
}
INT_MAX_MINUS_ONE = 2147483646  # getConstants() currently reads a signed C int.


def byte_length(value):
    return len(value.encode("utf-8"))


def json_byte_length(data):
    """Conservative UTF-8 JSON length, excluding quotes (json-c may escape '/')."""
    return sum(6 if byte < 32 else 2 if byte in (34, 47, 92) else 1 for byte in data)


def read_almond_config(path):
    """Keep every assignment: an earlier, longer value must also fit at startup."""
    if not path.exists():
        return None
    history = {}
    with path.open(encoding="utf-8") as source:
        for number, raw in enumerate(source, 1):
            row = raw.strip()
            if not row or row.startswith("#"):
                continue
            if "=" not in row or "\0" in row:
                raise ValueError("{}:{}: expected key=value".format(path, number))
            key, value = (part.strip() for part in row.split("=", 1))
            if not key:
                raise ValueError("{}:{}: empty configuration key".format(path, number))
            history.setdefault(key, []).append(value)
    return history


def sample_file_size(path, longest_line=False):
    total, longest, pending = 0, 0, 0
    with path.open("rb") as source:
        while True:
            data = source.read(65536)
            if not data:
                break
            if b"\0" in data:
                raise ValueError("NUL byte in text capture: {}".format(path))
            total += len(data)
            if longest_line:
                parts = data.split(b"\n")
                if len(parts) == 1:
                    pending += len(data)
                else:
                    longest = max(longest, pending + len(parts[0]) + 1,
                                  max((len(part) + 1 for part in parts[1:-1]), default=0))
                    pending = len(parts[-1])
    if not total:
        raise ValueError("empty runtime capture: {}".format(path))
    return max(longest, pending) if longest_line else total


def runtime_samples(manifest):
    if manifest is None:
        return {}
    with manifest.open(encoding="utf-8") as source:
        mapping = json.load(source)
    if not isinstance(mapping, dict):
        raise ValueError("runtime-samples manifest must be an object mapping keys to file lists")
    samples = {}
    for key, paths in mapping.items():
        if key not in RUNTIME_KEYS:
            raise ValueError("unsupported runtime sample key: {}".format(key))
        if not isinstance(paths, list) or not paths or any(not isinstance(p, str) or not p for p in paths):
            raise ValueError("{} must have a nonempty list of sample paths".format(key))
        samples[key] = max(sample_file_size(manifest.parent / p,
                                           key in {"LOGMESSAGE_SIZE", "INFOSTR_SIZE"})
                           for p in paths)
    return samples


def output_capacity(observed, args):
    return max(64, math.ceil(observed * (1 + args.margin_percent / 100)) + args.output_reserve)


def expanded_plan(args, plugins, config, measured, existing, captures, escaped_output):
    plan = {key: existing.get(key, default) for key, default in DEFAULTS.items()}
    basis = {key: "retained: no representative capture" for key in KEYS}
    observations = dict(measured)
    for key in PLUGIN_KEYS:
        if key in measured:
            plan[key] = (output_capacity(measured[key], args) if key in PLUGIN_KEYS[4:]
                         else max(1, measured[key]))
            basis[key] = "sampled stdout" if key in PLUGIN_KEYS[4:] else "declarations"

    def set_size(key, size, reason):
        observations[key] = size
        plan[key] = max(1, size)
        basis[key] = reason

    def max_len(values):
        return max(byte_length(value) for value in values)

    # CLI paths are the exact strings sampled; include configured duplicates too.
    dirs = [str(args.plugin_dir)] + ((config or {}).get("plugins.directory", []))
    declarations = [str(args.plugins_conf)] + ((config or {}).get("plugins.declaration", []))
    set_size("PLUGINDIR_SIZE", max_len(dirs), "plugin directory")
    set_size("PLUGINDECLARATIONFILE_SIZE", max_len(declarations), "declaration path")
    context = {}
    if config is not None:
        for key, (setting, initial) in PATH_FIELDS.items():
            candidates = list(config.get(setting, []))
            if initial is not None:
                candidates.append(initial)
            if candidates:
                context[key] = candidates
                set_size(key, max_len(candidates), "config + startup defaults")
            else:
                basis[key] = "retained: {} missing".format(setting)
        hosts = list(config.get("scheduler.hostName", []))
        if not hosts or hosts[-1] == "None":
            hosts += [socket.gethostname(), socket.getfqdn()]
        hosts.append("None")
        context["HOSTNAME_SIZE"] = hosts
        set_size("HOSTNAME_SIZE", max_len(hosts), "configured/local hostname")

        # Length arithmetic mirrors C '%s/%s', including any trailing slashes.
        def length(key):
            return max_len(context[key])

        if "LOGDIR_SIZE" in context:
            set_size("LOGFILE_SIZE", max(length("LOGDIR_SIZE") + len("/almond.log"),
                                          len("/var/log/almond/almond.log")), "log path + bootstrap fallback")
        if "DATADIR_SIZE" in context:
            set_size("DATAFILENAME_SIZE", length("DATADIR_SIZE") + 1 + length("JSONFILENAME_SIZE"),
                     "data directory / JSON name")
            set_size("BACKUPDIRECTORY_SIZE", length("DATADIR_SIZE") + len("/backup"),
                     "data directory / backup")
            set_size("NEWFILENAME_SIZE", plan["BACKUPDIRECTORY_SIZE"] + 1 +
                     length("JSONFILENAME_SIZE") + 1 + 12, "backup / JSON name . YYYYMMDDHHMM")
        if "STOREDIR_SIZE" in context:
            set_size("STORENAME_SIZE", length("STOREDIR_SIZE") + 1 + length("METRICSFILENAME_SIZE"),
                     "store directory / metrics name")
        if all(key in context for key in ("DATADIR_SIZE", "STOREDIR_SIZE", "LOGDIR_SIZE")):
            host_prefix = max(byte_length(host.split(".", 1)[0]) for host in hosts)
            filename = max(plan["DATAFILENAME_SIZE"], plan["LOGFILE_SIZE"],
                           length("DATADIR_SIZE") + len("/store_files.tar.gz"),
                           length("STOREDIR_SIZE") + 1 + host_prefix + 1 + measured["PLUGINITEMNAME_SIZE"],
                           length("JSONFILENAME_SIZE"))
            set_size("FILENAME_SIZE", filename, "longest data/log/CSV/archive path")
        else:
            basis["FILENAME_SIZE"] = "retained: incomplete directory configuration"
    else:
        for key in set(PATH_FIELDS) | {"HOSTNAME_SIZE", "FILENAME_SIZE", "LOGFILE_SIZE",
                                      "DATAFILENAME_SIZE", "BACKUPDIRECTORY_SIZE", "NEWFILENAME_SIZE",
                                      "STORENAME_SIZE"}:
            basis[key] = "retained: almond.conf unavailable"

    # These account for examined formats. Inputs arriving after startup can
    # still be larger; print 'model' rather than presenting them as measurements.
    config_longest = max((byte_length(value) for values in (config or {}).values()
                          for value in values), default=0)
    path_longest = max((observations.get(key, 0) for key in PATH_FIELDS), default=0)
    info = max(DEFAULTS["INFOSTR_SIZE"], measured["PLUGINCOMMAND_SIZE"] + 64,
               measured["PLUGINITEMNAME_SIZE"] + 128, config_longest + 128, path_longest + 128,
               captures.get("INFOSTR_SIZE", 0))
    observations["INFOSTR_SIZE"] = info
    plan["INFOSTR_SIZE"] = output_capacity(info, args)
    basis["INFOSTR_SIZE"] = "model: formats/config/commands + capture"
    log = max(DEFAULTS["LOGMESSAGE_SIZE"], plan["INFOSTR_SIZE"] + 32)
    output_logging = (config or {}).get("scheduler.logPluginOutput", ["false"])[-1].lower()
    if output_logging == "true" or (output_logging.isdigit() and int(output_logging) > 0):
        log = max(log, measured["PLUGINITEMNAME_SIZE"] + 3 + plan["PLUGINOUTPUT_SIZE"] + 32)
    log = max(log, captures.get("LOGMESSAGE_SIZE", 0))
    observations["LOGMESSAGE_SIZE"] = log
    plan["LOGMESSAGE_SIZE"] = output_capacity(log, args)
    basis["LOGMESSAGE_SIZE"] = "model: logger header/messages + capture"
    if not args.static_only:
        json_name = max(json_byte_length(p.name.encode("utf-8")) for p in plugins)
        json_description = max(json_byte_length(p.description.encode("utf-8")) for p in plugins)
        api = max(DEFAULTS["APIMESSAGE_SIZE"],
                  2 * json_name + json_description + max(output_capacity(escaped_output, args), 500) + 1536,
                  captures.get("APIMESSAGE_SIZE", 0))
        observations["APIMESSAGE_SIZE"] = api
        plan["APIMESSAGE_SIZE"] = output_capacity(api, args)
        basis["APIMESSAGE_SIZE"] = "model: verbose JSON/escaping/history + capture"
    for key, observed in captures.items():
        if key in {"INFOSTR_SIZE", "LOGMESSAGE_SIZE", "APIMESSAGE_SIZE"}:
            continue
        observations[key] = observed
        plan[key] = output_capacity(observed, args)
        basis[key] = "sample-file bytes"
    if args.grow_only:
        plan = {key: max(value, existing.get(key, 0)) for key, value in plan.items()}
    return plan, observations, basis


def read_plugins(path):
    plugins = []
    with path.open("r", encoding="utf-8", newline="") as source:
        for number, raw in enumerate(source, 1):
            if not raw.strip() or raw.lstrip().startswith("#"):
                continue
            if "\0" in raw:
                raise ValueError("{}:{}: NUL byte in declaration".format(path, number))
            match = re.fullmatch(r"\[([^\]]+)\] *(.*)", raw.rstrip("\r\n"))
            if not match:
                raise ValueError("{}:{}: expected [name] description;command;active;interval".format(
                    path, number))
            # Match C strtok's literal semicolon splitting; do not interpret
            # shell quotes or JSON labels as an alternative declaration format.
            fields = match.group(2).split(";")
            if len(fields) < 4 or any(not field for field in fields[:4]):
                raise ValueError("{}:{}: incomplete declaration".format(path, number))
            description, command, active, interval = fields[:4]
            if not re.fullmatch(r"\s*[+-]?\d+\s*", active):
                raise ValueError("{}:{}: active must be an integer; semicolons "
                                 "inside commands are not supported by Almond".format(path, number))
            if not re.fullmatch(r"\s*[+-]?\d+\s*", interval.split("{", 1)[0]):
                raise ValueError("{}:{}: invalid interval".format(path, number))
            plugins.append(Plugin(match.group(1), description, command, int(active), number))
    if not plugins:
        raise ValueError("no plugin declarations found in {}".format(path))
    return plugins


def read_optional(path):
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return None


def parse_memalloc(original):
    text = (original or b"").decode("utf-8")
    values = {}
    for number, row in enumerate(text.splitlines(), 1):
        if not row.strip():
            continue
        match = re.fullmatch(r"\s*([A-Z][A-Z0-9_]*)\s+([0-9]+)\s*", row)
        if not match:
            raise ValueError("memalloc row {}: expected KEY positive_integer; "
                             "Almond's fscanf reader does not accept comments".format(number))
        key, value = match.group(1), int(match.group(2))
        if byte_length(key) > 49 or not 1 <= value <= INT_MAX_MINUS_ONE:
            raise ValueError("memalloc row {}: key or value outside C reader limits".format(number))
        if key in values:
            raise ValueError("duplicate memalloc key: {}".format(key))
        values[key] = value
    if len(values) > 50:
        raise ValueError("memalloc.alm exceeds the C reader's MAX_CONSTANTS=50")
    return text, values


def full_command(plugin_dir, plugin):
    # Deliberately match snprintf(cmd, ..., "%s/%s", pluginDir, item->command).
    # Do not shlex.split(), re-quote, or substitute a different executable.
    return str(plugin_dir) + "/" + plugin.command


def measure(command, timeout, max_output, cwd, statistics=None):
    started = time.monotonic()
    proc = subprocess.Popen(command, shell=True, executable="/bin/sh",
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, start_new_session=True,
                            cwd=cwd, bufsize=0)
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ, "stdout")
    selector.register(proc.stderr, selectors.EVENT_READ, "stderr")
    counts = {"stdout": 0, "stderr": 0}
    problem = None
    escaped_output = 0
    try:
        while selector.get_map() or proc.poll() is None:
            remaining = timeout - (time.monotonic() - started)
            if remaining <= 0:
                problem = "timed out after {} seconds".format(timeout)
                break
            if not selector.get_map():
                try:
                    proc.wait(timeout=remaining)
                except subprocess.TimeoutExpired:
                    problem = "timed out after {} seconds".format(timeout)
                break
            for key, _ in selector.select(min(remaining, 0.2)):
                data = os.read(key.fd, 65536)
                if not data:
                    selector.unregister(key.fileobj)
                    key.fileobj.close()
                    continue
                counts[key.data] += len(data)
                if key.data == "stdout":
                    if statistics is not None:
                        escaped_output += json_byte_length(data)
                    if b"\0" in data:
                        problem = "NUL byte in stdout; Almond expects text"
                    if counts["stdout"] > max_output:
                        problem = "stdout exceeded --max-output-bytes={} (sample incomplete)".format(
                            max_output)
                if problem:
                    break
            if problem:
                break
        if problem:
            raise ValueError(problem)
        rc = proc.wait()
        if rc not in (0, 1, 2, 3):
            raise ValueError("exit code {} is not a monitoring status (0..3); "
                             "{} stderr bytes".format(rc, counts["stderr"]))
        if counts["stdout"] == 0:
            raise ValueError("empty stdout (exit {}, {} stderr bytes); cannot size output".format(
                rc, counts["stderr"]))
        if statistics is not None:
            statistics["json_bytes"] = escaped_output
        return counts["stdout"], counts["stderr"], rc
    except BaseException:
        # The shell and its descendants share this new process group. Kill all
        # of them, including descendants still holding the capture pipes open.
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.wait()
        raise
    finally:
        selector.close()
        proc.stdout.close()
        proc.stderr.close()


def render_memalloc(text, proposed):
    remaining = dict(proposed)
    lines = []
    for row in text.splitlines(keepends=True):
        match = re.match(r"(\s*)([A-Z][A-Z0-9_]*)(\s+)([0-9]+)(.*)", row.rstrip("\r\n"))
        if match and match.group(2) in remaining:
            key = match.group(2)
            ending = row[len(row.rstrip("\r\n")):]
            row = "{}{}{}{}{}{}".format(match.group(1), key, match.group(3),
                                         remaining.pop(key), match.group(5), ending)
        lines.append(row)
    result = "".join(lines)
    if remaining and result and not result.endswith(("\n", "\r")):
        result += "\n"
    result += "".join("{} {}\n".format(key, value) for key, value in remaining.items())
    parse_memalloc(result.encode("utf-8"))  # Includes MAX_CONSTANTS check after appends.
    return result.encode("utf-8")


def atomic_write(path, content, original):
    # Resolve an existing symlink so the link itself survives replacement.
    path = path.resolve()
    if read_optional(path) != original:
        raise ValueError("{} changed during sampling; run again".format(path))
    if content == original:
        print("Unchanged: {}".format(path))
        return
    info = path.stat() if original is not None else None
    if info and not stat.S_ISREG(info.st_mode):
        raise ValueError("not a regular file: {}".format(path))
    descriptor, temporary = tempfile.mkstemp(prefix="." + path.name + ".", dir=str(path.parent))
    try:
        with os.fdopen(descriptor, "wb") as target:
            if info and (info.st_uid != os.geteuid() or info.st_gid != os.getegid()):
                os.fchown(target.fileno(), info.st_uid, info.st_gid)
            os.fchmod(target.fileno(), stat.S_IMODE(info.st_mode) if info else 0o644)
            target.write(content)
            target.flush()
            os.fsync(target.fileno())
        if read_optional(path) != original:
            raise ValueError("{} changed before writing; run again".format(path))
        if original is not None:
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            backup = path.with_name(path.name + ".bak." + stamp + "." + str(os.getpid()))
            with backup.open("xb") as target:
                os.fchmod(target.fileno(), stat.S_IMODE(info.st_mode))
                target.write(original)
                target.flush()
                os.fsync(target.fileno())
            print("Backup: {}".format(backup))
        os.replace(temporary, str(path))
        print("Updated: {}".format(path))
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--almond-conf", type=Path, default=Path("/etc/almond/almond.conf"))
    parser.add_argument("--plugins-conf", type=Path, help="default: plugins.declaration in almond.conf")
    parser.add_argument("--plugin-dir", default=None,
                        help="exact plugins.directory value from almond.conf")
    parser.add_argument("--memalloc", type=Path, default=Path("/opt/almond/memalloc.alm"))
    parser.add_argument("--enable-conf", type=Path, default=Path("/etc/almond/memalloc.conf"))
    parser.add_argument("--cwd", type=Path, help="working directory for plugin commands; default: current")
    parser.add_argument("--samples", type=int, default=1, help="runs per selected plugin (default: 1)")
    parser.add_argument("--timeout", type=float, default=30, help="seconds per run (default: 30)")
    parser.add_argument("--margin-percent", type=float, default=20, help="output headroom (default: 20)")
    parser.add_argument("--output-reserve", type=int, default=64, help="extra output bytes (default: 64)")
    parser.add_argument("--max-output-bytes", type=int, default=1048576,
                        help="stop a run above this stdout limit (default: 1048576)")
    parser.add_argument("--include-inactive", action="store_true", help="also execute inactive plugins")
    parser.add_argument("--runtime-samples", type=Path, help="JSON map of runtime keys to capture-file paths")
    parser.add_argument("--grow-only", action="store_true", help="retain any larger existing managed size")
    parser.add_argument("--static-only", action="store_true", help="preview static sizes without running plugins")
    parser.add_argument("--apply", action="store_true", help="back up and update memalloc.alm")
    parser.add_argument("--enable", action="store_true", help="also enable memalloc.conf; requires --apply")
    args = parser.parse_args(argv)
    if args.samples < 1 or not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--samples and --timeout must be positive")
    if not math.isfinite(args.margin_percent) or args.margin_percent < 0 or args.output_reserve < 0:
        parser.error("margin and reserve must be finite and non-negative")
    if not 1 <= args.max_output_bytes <= INT_MAX_MINUS_ONE:
        parser.error("invalid output limit")
    if args.enable and not args.apply:
        parser.error("--enable requires --apply")
    if args.static_only and args.apply:
        parser.error("--static-only is preview-only; it cannot be used with --apply")
    return args


def main(argv=None):
    args = arguments(argv)
    config_original = read_optional(args.almond_conf)
    config = read_almond_config(args.almond_conf)
    args.plugins_conf = args.plugins_conf or Path((config or {}).get(
        "plugins.declaration", ["/etc/almond/plugins.conf"])[-1])
    args.plugin_dir = args.plugin_dir if args.plugin_dir is not None else (config or {}).get(
        "plugins.directory", ["/opt/almond/plugins"])[-1]
    if config is None:
        print("almond.conf unavailable: config-dependent sizes will be retained.")
    captures = runtime_samples(args.runtime_samples)
    plugins_original = read_optional(args.plugins_conf)
    plugins = read_plugins(args.plugins_conf)
    original = read_optional(args.memalloc)
    text, existing = parse_memalloc(original)
    enable_original = read_optional(args.enable_conf) if args.enable else None
    if args.enable and args.enable_conf.resolve() == args.memalloc.resolve():
        raise ValueError("--enable-conf and --memalloc must be different files")
    measured = dict(zip(PLUGIN_KEYS[:4], (
        max(byte_length(p.name) for p in plugins),
        max(byte_length(p.description) for p in plugins),
        max(byte_length(p.command) for p in plugins),
        max(byte_length(full_command(args.plugin_dir, p)) for p in plugins))))
    selected = [p for p in plugins if p.active == 1 or args.include_inactive]
    failures = []
    largest_output = 0
    largest_escaped_output = 0
    if not args.static_only:
        if not selected:
            raise ValueError("no active plugins to sample; use --include-inactive if appropriate")
        print("Executing {} plugins, {} sample(s) each; stdout measured in bytes.".format(
            len(selected), args.samples), flush=True)
        for plugin in selected:
            for sample in range(1, args.samples + 1):
                label = "{!r} [{}/{}]".format(plugin.name, sample, args.samples)
                print("Sampling {} ...".format(label), flush=True)
                try:
                    statistics = {}
                    output, errors, rc = measure(full_command(args.plugin_dir, plugin),
                                                  args.timeout, args.max_output_bytes, args.cwd, statistics)
                    largest_output = max(largest_output, output)
                    largest_escaped_output = max(largest_escaped_output, statistics["json_bytes"])
                    print("  stdout={} bytes; stderr={} bytes; exit={}".format(output, errors, rc), flush=True)
                except (OSError, ValueError) as error:
                    failures.append(label + ": " + str(error))
                    print("  FAILED: {}".format(error), flush=True)
        if failures:
            raise ValueError("{} incomplete sample(s); no files written. Fix failed "
                             "commands/environment or adjust the timeout/limit, then rerun.".format(len(failures)))
        measured["PLUGINOUTPUT_SIZE"] = largest_output
        measured["PLUGINMESSAGE_SIZE"] = largest_output
    proposed, observed, basis = expanded_plan(args, plugins, config, measured, existing,
                                              captures, largest_escaped_output)
    if any(value > INT_MAX_MINUS_ONE for value in proposed.values()):
        raise ValueError("a proposed size exceeds the C reader's signed int limit")
    updated = render_memalloc(text, proposed)
    print("\n{:<28} {:>10} {:>10} {:>10}  {}".format("Key", "Basis bytes", "Existing", "Proposed", "Sizing basis"))
    for key in KEYS:
        if key in proposed:
            print("{:<28} {:>10} {:>10} {:>10}  {}".format(
                key, observed.get(key, "-"), existing.get(key, "default"), proposed[key], basis[key]))
    retained = [key for key in KEYS if basis[key].startswith("retained:")]
    print("\n{} of {} entries calculated; {} retained. Unknown keys are preserved.".format(
        len(KEYS) - len(retained), len(KEYS), len(retained)))
    print("Stored values exclude Almond's added NUL byte. Models and samples cannot prove future maxima.")
    if len(selected) < len(plugins):
        print("Inactive plugins were included in static sizes but were not executed.")
    if args.static_only:
        print("Static preview only: output sizes were not measured; no commands executed or files written.")
    elif not args.apply:
        print("Preview only: commands ran, but no files were written. Use --apply to save; --enable to activate.")
    else:
        # Refuse to overwrite a declaration file through a mistaken CLI path.
        protected = {args.plugins_conf.resolve(), args.almond_conf.resolve()}
        if args.runtime_samples:
            protected.add(args.runtime_samples.resolve())
        if args.memalloc.resolve() in protected or (args.enable and args.enable_conf.resolve() in protected):
            raise ValueError("output paths must not overwrite input configuration files")
        if read_optional(args.plugins_conf) != plugins_original or read_optional(args.almond_conf) != config_original:
            raise ValueError("Almond/plugin configuration changed during sampling; run again")
        atomic_write(args.memalloc, updated, original)
        if args.enable:
            atomic_write(args.enable_conf, b"enabled\n", enable_original)
        print("Restart Almond to use these sizes. Output headroom covers samples, not every future state.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted; current command group stopped.", file=sys.stderr)
        sys.exit(130)
    except (OSError, ValueError, OverflowError) as error:
        print("ERROR: {}".format(error), file=sys.stderr)
        sys.exit(1)
