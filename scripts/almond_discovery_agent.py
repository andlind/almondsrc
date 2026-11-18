#!/usr/bin/env python3
"""Generate an initial Almond monitoring configuration for Linux.

By default print the configuration to stdout and diagnostics to stderr.
--apply writes the target; --force is required to replace an existing target
and first creates a timestamped backup. Generated checks are executed once
before output or installation. Matching inode checks are included when
check_disk supports them. Swap checks are added only with active swap.
Platform-aware selection prefers check_ping on physical/virtual machines and
check_icmp in containers, with execution-tested fallback. Container host-scope
metrics are suggestions by default. Almond self-monitoring uses check_process
when present rather than check_procs, which ignores its parent (the Almond
scheduler). check_almond_ps remains a separate API-health candidate. Run with the same user as Almond if possible.
See --help for options. Python 3.8+; standard library only.
"""
import argparse
import datetime
import hashlib
import ipaddress
import json
import math
import os
from pathlib import Path
import re
import signal
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import tempfile

HEADER = '# [service_name] description;plugin args;active;interval labels'
PSEUDO = {'proc', 'sysfs', 'devtmpfs', 'devpts', 'tmpfs', 'cgroup', 'cgroup2',
          'securityfs', 'pstore', 'debugfs', 'tracefs', 'configfs', 'fusectl',
          'mqueue', 'hugetlbfs', 'autofs', 'nsfs', 'bpf', 'binfmt_misc',
          'rpc_pipefs', 'efivarfs', 'ramfs', 'squashfs'}
# Exact Linux process names and recognized specialty-plugin names, never arbitrary commands.
SERVICES = {
    'postgresql': (('postgres', 'postmaster'), ('check_pgsql', 'check_postgres', 'check_postgres.pl', 'check_postgresql')),
    'mysql': (('mysqld', 'mariadbd'), ('check_mysql', 'check_mysql_health', 'check_mysql_health.pl')),
    'redis': (('redis-server',), ('check_redis', 'check_redis.py', 'check_redis.pl')),
    'nginx': (('nginx',), ('check_nginx', 'check_nginx.py', 'check_nginx.sh')),
    'apache': (('apache2', 'httpd'), ('check_apache', 'check_apache.py', 'check_http')),
    'haproxy': (('haproxy',), ('check_haproxy', 'check_haproxy.py')),
    'postfix': (('master',), ('check_postfix', 'check_postfix.pl')),
    'dovecot': (('dovecot',), ('check_dovecot', 'check_imap')),
    'exim': (('exim', 'exim4'), ('check_exim',)),
    'tomcat': (('java',), ('check_tomcat', 'check_tomcat.py')),
    'jetty': (('java',), ('check_jetty',)),
    'gunicorn': (('gunicorn', 'python', 'python3', 'python3.11', 'python3.12', 'python3.13'), ('check_gunicorn', 'check_http')),
    'supervisord': (('supervisord', 'python', 'python3', 'python3.11', 'python3.12', 'python3.13'), ('check_supervisord', 'check_supervisor')),
    'uwsgi': (('uwsgi',), ('check_uwsgi',)),
    'docker': (('dockerd',), ('check_docker', 'check_docker.py')),
    'containerd': (('containerd',), ('check_containerd',)),
    'mongodb': (('mongod',), ('check_mongodb', 'check_mongodb.py')),
    'memcached': (('memcached',), ('check_memcached',)),
    'prometheus': (('prometheus',), ('check_prometheus',)),
    'node_exporter': (('node_exporter',), ('check_node_exporter',)),
    'elasticsearch': (('java',), ('check_elasticsearch', 'check_elasticsearch.py')),
    'rabbitmq': (('beam.smp',), ('check_rabbitmq', 'check_rabbitmq.py')),
    'howru': (('python', 'python3', 'gunicorn'), ('check_howru_api.py',)),
    'redpanda': (('redpanda',), ('check_redpanda', 'check_kafka')),
    'minio': (('minio',), ('check_minio', 'check_minio.py')),
    'flume': (('java',), ('check_flume', 'check_flume.py')),
    'sybase': (('dataserver',), ('check_sybase', 'check_sybase.pl', 'check_sybase_health')),
    'nghttpx': (('nghttpx',), ('check_nghttpx',)),
    'chrony': (('chronyd',), ('check_chrony', 'check_ntp_time')),
    'dnsmasq': (('dnsmasq',), ('check_dns',)),
    'snmpd': (('snmpd',), ('check_snmp',)),
    'nrpe': (('nrpe',), ('check_nrpe',)),
    'collectd': (('collectd',), ('check_collectd',)),
    'rsyslog': (('rsyslogd',), ('check_rsyslog',)),
    'kafka': (('java',), ('check_kafka', 'check_kafka.py')),
    'sshd': (('sshd',), ('check_ssh',)),
    'almond': (('almond',), ('check_almond_ps',)),
}
# Strings must be present in the actual command line for ambiguous process names.
CMD_REQUIRED = {'postfix': ('postfix',), 'tomcat': ('catalina', 'tomcat'),
                'jetty': ('jetty',), 'kafka': ('kafka.kafka', 'kafka.kafkaservers', 'kafka.kraft'),
                'elasticsearch': ('elasticsearch',), 'rabbitmq': ('rabbitmq',),
                'howru': ('howru.py', 'howru:app', 'howru.app'),
                'flume': ('org.apache.flume.node.application',)}
SAFE_NAME = re.compile(r'^[A-Za-z0-9_.+-]+$')
MOUNT_SAFE = re.compile(r'^/[A-Za-z0-9_./+@=-]*$')


def read_text(path):
    try:
        return Path(path).read_text(errors='replace').strip()
    except OSError:
        return ''


def command_output(args):
    try:
        p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                           text=True, timeout=2, check=False)
        return p.stdout.strip() if p.returncode == 0 else ''
    except (OSError, subprocess.TimeoutExpired):
        return ''


def detect_platform():
    """Best-effort Linux platform classification, independent of plugin choices.

    A container may expose the host DMI; the container classification takes
    precedence. Physical vs undetected VM is inherently uncertain.
    """
    cgroups = (read_text('/proc/1/cgroup') + '\n' +
               read_text('/proc/self/cgroup')).lower()
    container = ''
    if os.environ.get('KUBERNETES_SERVICE_HOST') and os.environ.get('KUBERNETES_SERVICE_PORT'):
        container = 'kubernetes'
    elif Path('/run/.containerenv').exists():
        container = 'podman'
    elif Path('/.dockerenv').exists():
        container = 'docker'
    if not container:
        for word, label in (('kubepods', 'kubernetes'), ('containerd', 'containerd'),
                            ('docker', 'docker'), ('libpod', 'podman'),
                            ('podman', 'podman'), ('lxc', 'lxc')):
            if word in cgroups:
                container = label
                break
    if not container:
        detected = command_output(['systemd-detect-virt', '--container'])
        if detected and detected != 'none':
            container = detected

    vendor = (read_text('/sys/class/dmi/id/sys_vendor') + ' ' +
              read_text('/sys/class/dmi/id/product_name')).strip().lower()
    vm = command_output(['systemd-detect-virt', '--vm'])
    if not vm or vm == 'none':
        for token, label in (('vmware', 'vmware'), ('virtualbox', 'virtualbox'),
                             ('qemu', 'kvm/qemu'), ('kvm', 'kvm'), ('xen', 'xen'),
                             ('hyper-v', 'hyper-v'), ('microsoft corporation', 'hyper-v'),
                             ('parallels', 'parallels'), ('bhyve', 'bhyve')):
            if token in vendor:
                vm = label
                break

    if container:
        kind = 'container'
        detail = 'container: ' + container
        if vm:
            detail += '; host DMI/virtualisation indicates ' + vm + ' (may refer to container host)'
    elif vm:
        kind = 'vm'
        detail = 'virtual machine: ' + vm
    elif vendor and not any(x in vendor for x in ('virtual', 'unknown', 'not specified')):
        kind = 'physical'
        detail = 'probably physical server (DMI: ' + vendor + ')'
    else:
        kind = 'unknown'
        detail = 'unknown (physical server or undetected VM)'
    return {'kind': kind, 'container': container, 'hypervisor': vm or '', 'detail': detail}


def platform_profile(override='auto'):
    """Respect an explicit operator override while preserving detected details."""
    profile = detect_platform()
    if override != 'auto':
        profile = dict(profile)
        profile['kind'] = override
        profile['detail'] += ' [monitoring policy overridden to ' + override + ']'
    return profile


def environment():
    """Keep the original human-readable inventory function available."""
    return detect_platform()['detail']


def preferred_ping_plugins(profile, preference='auto'):
    """Order is a policy preference, not an assumption about permissions."""
    if preference == 'check_ping':
        return ('check_ping', 'check_icmp')
    if preference == 'check_icmp':
        return ('check_icmp', 'check_ping')
    if profile['kind'] == 'container':
        return ('check_icmp', 'check_ping')
    return ('check_ping', 'check_icmp')


def local_ping_command(plugin):
    return plugin + ' -H 127.0.0.1 -w 100,20% -c 500,60%'


def select_local_ping(plugin_dir, profile, timeout, preference='auto'):
    """Test preferred, then fallback; require OK before enabling a loopback check.

    A non-zero Nagios plugin exit (WARNING/CRITICAL) isn't sufficient for a
    local loopback baseline. Always run under the intended Almond user.
    """
    available = plugins(plugin_dir)
    messages = []
    for name in preferred_ping_plugins(profile, preference):
        if name not in available:
            messages.append(name + ' unavailable')
            continue
        valid, status, message = test_check(plugin_dir, local_ping_command(name), timeout)
        print('PING CANDIDATE {}: {} - {}'.format(name, status, message),
              file=sys.stderr, flush=True)
        if valid and status == 'OK':
            messages.append(name + ' verified OK')
            return {'plugin': name, 'messages': messages}
        clean = ' '.join(message.split())[:150]
        messages.append('{} failed {}: {}'.format(name, status, clean))
    return {'plugin': None, 'messages': messages}


def default_gateway_ipv4(route_file='/proc/net/route'):
    """Identify an existing IPv4 default route, never assume reachability."""
    candidates = []
    for line in read_text(route_file).splitlines()[1:]:
        parts = line.split()
        if len(parts) < 8:
            continue
        iface, dest, gateway, flags = parts[:4]
        try:
            metric = int(parts[6])
            if dest != '00000000' or not (int(flags, 16) & 0x3) == 0x3:
                continue
            address = socket.inet_ntoa(struct.pack('<I', int(gateway, 16)))
            ip = ipaddress.IPv4Address(address)
            if ip.is_unspecified or ip.is_loopback or ip.is_multicast:
                continue
        except (ValueError, OSError, OverflowError, struct.error):
            continue
        candidates.append((metric, iface, address))
    return min(candidates)[2] if candidates else None


def mountpoints(path='/proc/self/mountinfo'):
    found = set()
    for line in read_text(path).splitlines():
        before, sep, after = line.partition(' - ')
        left, right = before.split(), after.split()
        if not sep or len(left) < 5 or len(right) < 2:
            continue
        mount = re.sub(r'\\([0-7]{3})', lambda m: chr(int(m.group(1), 8)), left[4])
        fs = right[0]
        if not MOUNT_SAFE.fullmatch(mount) or not os.path.isdir(mount):
            continue
        if mount != '/' and (fs in PSEUDO or mount.startswith(('/proc/', '/sys/', '/dev/', '/run/'))
                             or mount in ('/proc', '/sys', '/dev', '/run')):
            continue
        found.add(mount)
    return sorted(found, key=lambda p: (p != '/', p))


def active_swap(path='/proc/swaps'):
    """True only when /proc/swaps lists at least one configured swap device/file."""
    lines = read_text(path).splitlines()
    return any(len(line.split()) >= 3 for line in lines[1:])


def processes(proc='/proc'):
    result = []
    try:
        entries = list(Path(proc).iterdir())
    except OSError:
        return result
    for entry in entries:
        if not entry.name.isdigit():
            continue
        name = read_text(entry / 'comm')
        try:
            cmd = (entry / 'cmdline').read_bytes().replace(b'\x00', b' ').decode('utf-8', 'replace').strip()
        except OSError:
            cmd = ''
        if name and cmd:  # avoid counting zombies and kernel threads
            result.append((name, cmd))
    return result


def detected_services(procs):
    found = {}
    for service, (names, _) in SERVICES.items():
        matches = []
        for name, cmd in procs:
            if service in ('supervisord', 'gunicorn') and name.startswith('python'):
                if not re.search(r'(^|[/\s])' + service + r'(?:\s|$|\.py\b)', cmd, re.I):
                    continue
            name_matches = name in names or (service in ('supervisord', 'gunicorn') and
                                              re.fullmatch(r'python(?:[0-9.]+)?', name))
            if name_matches and (service not in CMD_REQUIRED or
                                  any(s in cmd.lower() for s in CMD_REQUIRED[service])):
                matches.append((name, cmd))
        if matches:
            found[service] = matches
    return found


def python_apps(procs):
    """Identify Python script paths; a script name alone does not prove it is a web app."""
    found = set()
    for name, cmd in procs:
        if not re.fullmatch(r'python(?:[0-9.]+)?', name):
            continue
        for token in cmd.split()[1:]:
            if (token.startswith('/') and token.endswith('.py') and MOUNT_SAFE.fullmatch(token)
                    and not token.startswith(('/usr/bin/', '/usr/lib/', '/usr/lib64/'))
                    and token != os.path.abspath(__file__)):
                found.add((name, token))
                break
    return sorted(found)


def gunicorn_bind_ports(matches):
    """Extract explicitly configured IPv4 binds; only test ports also observed listening."""
    found = set()
    for _name, cmd in matches:
        try:
            args = shlex.split(cmd)
        except ValueError:
            continue
        for i, arg in enumerate(args):
            value = (args[i + 1] if arg in ('-b', '--bind') and i + 1 < len(args)
                     else arg.split('=', 1)[1] if arg.startswith('--bind=') else '')
            match = re.fullmatch(r'(?:0\.0\.0\.0|127\.0\.0\.1|localhost):(\d{1,5})', value)
            if match and 0 < int(match.group(1)) <= 65535:
                found.add(int(match.group(1)))
    return found


def plugins(directory):
    path = Path(directory)
    if not path.is_dir():
        raise ValueError('Plugin directory does not exist: ' + directory)
    found = {}
    for item in path.iterdir():
        if SAFE_NAME.fullmatch(item.name) and item.is_file() and os.access(item, os.X_OK):
            found[item.name] = item
    return found


def local_listening_ports(path='/proc/net/tcp'):
    ports = set()
    for line in read_text(path).splitlines()[1:]:
        fields = line.split()
        if len(fields) < 4 or fields[3] != '0A':
            continue
        try:
            addr, port = fields[1].split(':')
            # /proc/net/tcp stores IPv4 addresses as little-endian bytes.
            ip = ipaddress.IPv4Address(bytes.fromhex(addr)[::-1])
            if ip.is_unspecified or ip.is_loopback:
                ports.add(int(port, 16))
        except ValueError:
            continue
    return ports


def systemd_inventory():
    """Return running/failed service details; available inside containers if systemd runs."""
    args = ['systemctl', 'list-units', '--type=service', '--state=running,failed',
            '--no-legend', '--plain', '--no-pager']
    try:
        result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, timeout=10, check=False,
                                env=dict(os.environ, LC_ALL='C', SYSTEMD_COLORS='0'))
    except (OSError, subprocess.TimeoutExpired):
        return [], 'systemd discovery unavailable'
    if result.returncode:
        return [], 'systemd discovery unavailable (systemctl could not contact systemd)'
    units = []
    for line in result.stdout.splitlines():
        parts = line.lstrip('● ').split(None, 4)
        if len(parts) >= 4 and re.fullmatch(r'[A-Za-z0-9_.@:-]+\.service', parts[0]):
            units.append({'unit': parts[0], 'active': parts[2], 'sub': parts[3],
                          'pid': 0, 'type': ''})
    if not units:
        return [], ''
    # Batch property queries; retain the listing even when a unit disappears mid-run.
    by_name = {entry['unit']: entry for entry in units}
    for offset in range(0, len(units), 100):
        names = [u['unit'] for u in units[offset:offset + 100]]
        args = ['systemctl', 'show', '--no-pager', '--property=Id,MainPID,Type,ActiveState,SubState'] + names
        try:
            result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                    text=True, timeout=15, check=False,
                                    env=dict(os.environ, LC_ALL='C', SYSTEMD_COLORS='0'))
        except (OSError, subprocess.TimeoutExpired):
            continue
        for block in re.split(r'\n\s*\n', result.stdout.strip()):
            props = dict(line.split('=', 1) for line in block.splitlines() if '=' in line)
            entry = by_name.get(props.get('Id'))
            if entry:
                entry['pid'] = int(props.get('MainPID', '0')) if props.get('MainPID', '').isdigit() else 0
                entry['type'] = props.get('Type', '')
                entry['active'] = props.get('ActiveState', entry['active'])
                entry['sub'] = props.get('SubState', entry['sub'])
    return units, ''


def process_record(pid, proc='/proc'):
    path = Path(proc) / str(pid)
    comm = read_text(path / 'comm')
    try:
        argv = [x.decode('utf-8', 'replace') for x in (path / 'cmdline').read_bytes().split(b'\0') if x]
    except OSError:
        argv = []
    return {'pid': pid, 'comm': comm, 'argv': argv}


def socket_inventory(proc='/proc'):
    """Inventory local TCP listeners and bound UDP sockets, with best-effort owners."""
    listeners = []
    for proto in ('tcp', 'tcp6', 'udp', 'udp6'):
        for line in read_text(Path(proc) / 'net' / proto).splitlines()[1:]:
            fields = line.split()
            if len(fields) < 10 or (proto.startswith('tcp') and fields[3] != '0A'):
                continue
            # UDP state 07 means unconnected: these are bound sockets, not guaranteed servers.
            if proto.startswith('udp') and fields[3] != '07':
                continue
            try:
                addr, port = fields[1].split(':')
                raw = bytes.fromhex(addr)
                if len(raw) == 4:
                    ip = ipaddress.ip_address(raw[::-1])
                elif len(raw) == 16:
                    ip = ipaddress.ip_address(b''.join(raw[i:i+4][::-1] for i in range(0, 16, 4)))
                else:
                    continue
                port = int(port, 16)
                if not port:
                    continue
                listeners.append({'protocol': proto, 'address': str(ip), 'port': port,
                                  'scope': 'loopback' if ip.is_loopback else 'non-loopback/wildcard',
                                  'inode': fields[9], 'owners': []})
            except (ValueError, IndexError):
                continue
    inodes = {entry['inode'] for entry in listeners}
    owners = {}
    try:
        processes_dirs = list(Path(proc).iterdir())
    except OSError:
        processes_dirs = []
    denied = 0
    for procdir in processes_dirs:
        if not procdir.name.isdigit():
            continue
        try:
            fds = list((procdir / 'fd').iterdir())
        except (PermissionError, OSError):
            denied += 1
            continue
        found = set()
        for fd in fds:
            try:
                link = os.readlink(fd)
            except OSError:
                continue
            match = re.fullmatch(r'socket:\[(\d+)\]', link)
            if match and match.group(1) in inodes:
                found.add(match.group(1))
        if found:
            cg = read_text(procdir / 'cgroup')
            unit_match = re.search(r'(?:^|/)([A-Za-z0-9_.@:-]+\.service)(?:/|$)', cg, re.M)
            owner = {'pid': int(procdir.name), 'comm': read_text(procdir / 'comm'),
                     'unit': unit_match.group(1) if unit_match else ''}
            for inode in found:
                owners.setdefault(inode, []).append(owner)
    for entry in listeners:
        entry['owners'] = owners.get(entry['inode'], [])
    return sorted(listeners, key=lambda x: (x['protocol'], x['port'], x['address'])), denied


def selector_for_process(record):
    """Select a stable, non-secret command fragment for shared interpreters."""
    comm, argv = record['comm'], record['argv']
    if not comm or not SAFE_NAME.fullmatch(comm) or not argv:
        return None
    generic = re.fullmatch(r'(?:python[0-9.]*|java|ruby|node|nodejs|bash|sh|dash|agent)', comm)
    if not generic:
        return '-C ' + shlex.quote(comm)
    safe_token = re.compile(r'^[A-Za-z0-9_./+@=-]+$')
    if comm == 'java':
        for i, arg in enumerate(argv):
            if arg == '-jar' and i + 1 < len(argv) and safe_token.fullmatch(argv[i + 1]):
                return '-C java -a ' + shlex.quote(argv[i + 1])
            if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*){2,}', arg):
                return '-C java -a ' + shlex.quote(arg)
    else:
        for index, arg in enumerate(argv[1:], 1):
            if argv[index - 1] == '-m' and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.]*', arg):
                return '-C ' + shlex.quote(comm) + ' -a ' + shlex.quote(arg)
            if arg.startswith('/') and safe_token.fullmatch(arg):
                return '-C ' + shlex.quote(comm) + ' -a ' + shlex.quote(arg)
        if comm == 'agent' and argv[0].startswith('/') and safe_token.fullmatch(argv[0]):
            return '-C agent -a ' + shlex.quote(argv[0])
    return None


def runtime_candidates(procs):
    """Discover additional Java, Node and Python-module processes without logging argv."""
    candidates = set()
    for comm, cmd in procs:
        if not re.fullmatch(r'(?:java|node|nodejs|python[0-9.]*)', comm):
            continue
        try:
            argv = shlex.split(cmd)
        except ValueError:
            continue
        if comm.startswith('python') and '-m' not in argv:
            continue
        selector = selector_for_process({'comm': comm, 'argv': argv})
        if selector:
            candidates.add((comm, selector))
    return sorted(candidates)


def effective_cpu_count():
    """Best-effort effective CPUs from affinity and cgroup v2/v1 quota."""
    candidates = [float(os.cpu_count() or 1)]
    if hasattr(os, 'sched_getaffinity'):
        try:
            candidates.append(float(len(os.sched_getaffinity(0))))
        except OSError:
            pass
    quota = read_text('/sys/fs/cgroup/cpu.max').split()
    if len(quota) >= 2 and quota[0] != 'max':
        try:
            q, period = int(quota[0]), int(quota[1])
            if q > 0 and period > 0:
                candidates.append(q / period)
        except ValueError:
            pass
    else:
        try:
            q = int(read_text('/sys/fs/cgroup/cpu/cpu.cfs_quota_us'))
            period = int(read_text('/sys/fs/cgroup/cpu/cpu.cfs_period_us'))
            if q > 0 and period > 0:
                candidates.append(q / period)
        except ValueError:
            pass
    return max(0.1, min(candidates))


# These are suggestions, not verified plugin interfaces. Only propose known
# options; never fabricate credentials. Prefer an observed listening port.
SERVICE_PORTS = {
    'almond': 9909, 'postgresql': 5432, 'mysql': 3306,
    'redis': 6379, 'nginx': 80, 'apache': 80,
    'haproxy': 80, 'mongodb': 27017, 'memcached': 11211,
    'redpanda': 9092, 'kafka': 9092, 'rabbitmq': 5672,
    'elasticsearch': 9200, 'minio': 9000, 'sshd': 22,
    'dovecot': 143, 'sybase': 5000, 'oracle': 1521,
}


def specialty_suggestion(service, plugin, observed_ports):
    """Return a plausible command and explanation; never mark it active."""
    port = SERVICE_PORTS.get(service)
    if service == 'almond' and plugin == 'check_almond_ps':
        chosen = port if port in observed_ports else (sorted(observed_ports & {9909}) or [9909])[0]
        return '{} -H localhost -P {}'.format(plugin, chosen), 'verify Almond API port and access'
    if plugin == 'check_http':
        chosen = next((p for p in (80, 8080, 443, 8443) if p in observed_ports), 80)
        return 'check_http -H localhost -I 127.0.0.1 -p {}{}'.format(chosen, ' -S' if chosen in (443, 8443) else ''), 'verify HTTP endpoint and protocol'
    if plugin in ('check_pgsql', 'check_mysql', 'check_redis', 'check_ssh', 'check_tcp') and port:
        chosen = port if port in observed_ports else port
        if plugin == 'check_pgsql':
            return '{} -H 127.0.0.1 -p {}'.format(plugin, chosen), 'verify database and authentication'
        if plugin == 'check_mysql':
            return '{} -H 127.0.0.1 -P {}'.format(plugin, chosen), 'verify database credentials'
        if plugin == 'check_redis':
            return '{} -H 127.0.0.1 -p {}'.format(plugin, chosen), 'verify authentication and plugin syntax'
        if plugin == 'check_ssh':
            return '{} -H 127.0.0.1 -p {}'.format(plugin, chosen), 'verify SSH service'
    # Unknown interfaces: show the plugin name but do not invent flags.
    return plugin, 'arguments, credentials and endpoint must be configured from plugin help'


def generate(plugin_dir, procs=None, mounts=None, ports=None, units=None, sockets=None, proc_root='/proc', load_warning_factor=1.5, load_critical_factor=2.0, load_cpu_basis='host', swaps=None, labels=None, platform=None, ping_preference='auto', network_selection=None, container_host_metrics=False, gateway=None, gateway_suggestion=True):
    available = plugins(plugin_dir)
    if not available:
        raise ValueError('No executable plugins found in ' + plugin_dir)
    procs = processes() if procs is None else procs
    mounts = mountpoints() if mounts is None else mounts
    ports = local_listening_ports() if ports is None else ports
    detected = detected_services(procs)
    units = [] if units is None else units
    sockets = [] if sockets is None else sockets
    swaps = active_swap() if swaps is None else swaps
    labels = dict(labels or {})
    platform = platform if platform is not None else detect_platform()
    container = platform['kind'] == 'container'
    rows, notices = [], []
    notices.append('Platform: ' + platform['detail'])
    if container:
        notices.append('Container: process and network checks see container namespaces; filesystem usage may reflect host-backed storage.')
        if not container_host_metrics:
            notices.append('Container: host-scope load, memory, CPU, uptime, swap and open-file metrics kept as suggestions; use --container-host-metrics to enable explicitly.')
        if platform.get('container') == 'kubernetes':
            notices.append('Kubernetes: pod readiness, restarts and scheduling need external orchestration monitoring.')
    elif platform['kind'] == 'vm':
        notices.append('VM: these checks observe the guest OS; hypervisor datastore, host health and ballooning need external monitoring.')
    elif platform['kind'] == 'physical':
        notices.append('Physical server: add independent disk SMART/RAID, BMC and hardware sensors checks if available.')
    else:
        notices.append('Platform unknown: using physical/VM ping preference; use --platform to override.')
    failed_units = sorted(u.get('unit', 'unknown') for u in units
                          if u.get('active') == 'failed' or u.get('sub') == 'failed')
    for unit_name in failed_units[:20]:
        notices.append('ALERT: systemd unit {} is already failed; investigate before enabling monitoring.'.format(unit_name))
    if len(failed_units) > 20:
        notices.append('ALERT: {} additional failed units not listed.'.format(len(failed_units) - 20))
    suggestions = []
    used = set()

    def add(name, desc, command, interval, cls):
        if name in used:
            raise ValueError('Duplicate check name: ' + name)
        metadata = dict(labels)
        metadata['class'] = cls
        line = '[{}] {};{};1;{} {}'.format(name, desc, command, interval,
                                             json.dumps(metadata, separators=(',', ':')))
        if len(line.encode('utf-8')) >= 1023:
            notices.append('Skipped an overly long check: ' + name)
            return
        rows.append(line)
        used.add(name)

    # These interfaces are conventional or based on the supplied examples.
    # Local versions and thresholds should be reviewed before deployment.
    # Main uses network_selection (actual execution result); direct callers
    # get the preferred installed plugin and normal test_configuration still
    # validates it. No active line is created after failed preflight.
    if network_selection is not None:
        choice = network_selection['plugin']
        notices += ['Network probe: ' + message for message in network_selection['messages']]
    else:
        choice = next((p for p in preferred_ping_plugins(platform, ping_preference) if p in available), None)
    if choice:
        add('check_ping', 'Local ICMP (loopback only)', local_ping_command(choice), 1, 'network')
    else:
        notices.append('No verified local ICMP plugin; loopback check omitted. Confirm ICMP permissions and plugin installation.')
        # A helpful suggestion remains, rather than an invalid active check.
        for p in preferred_ping_plugins(platform, ping_preference):
            if p in available:
                suggestions.append('# Suggested: verify plugin permissions and options before enabling\n# [check_ping] Local ICMP;{};1;1 {}'.format(
                    local_ping_command(p), json.dumps(dict(labels, **{'class': 'network'}), separators=(',', ':'))))
    if 'check_disk' in available:
        for mount in mounts:
            if not MOUNT_SAFE.fullmatch(mount):
                notices.append('Skipped unsupported mount path: ' + repr(mount))
                continue
            suffix = 'root' if mount == '/' else re.sub(r'[^A-Za-z0-9]+', '_', mount.strip('/')).strip('_')
            name = 'check_disk_' + suffix
            if name in used:  # do not silently conflate mount points
                notices.append('Skipped mount with duplicate check name: ' + mount)
                continue
            add(name, 'Disk ' + mount, 'check_disk -w 10% -c 5% -p ' + shlex.quote(mount), 5, 'infra')
    else:
        notices.append('check_disk is unavailable; filesystem checks were not generated')
    def add_host_scope(name, desc, command, interval):
        if container and not container_host_metrics:
            # Do not present host-level procfs statistics as container health.
            suggestions.append('# Container-scope warning: may reflect host, not container limits\n# [{}] {};{};1;{} {}'.format(
                name, desc, command, interval,
                json.dumps(dict(labels, **{'class': 'system'}), separators=(',', ':'))))
        else:
            add(name, desc, command, interval, 'system')

    if 'check_load' in available:
        # /proc/loadavg commonly reflects the host scheduler in containers. Do
        # not divide by a container CPU quota unless the operator opts in.
        cpus = float(os.cpu_count() or 1) if load_cpu_basis == 'host' else effective_cpu_count()
        warn, crit = max(1.0, cpus * load_warning_factor), max(2.0, cpus * load_critical_factor)
        notices.append('Load: {:.2f} CPUs (basis={}), warning factor {:.2f}, critical factor {:.2f}; these are load averages, not CPU percentages. Review in containers.'.format(cpus, load_cpu_basis, load_warning_factor, load_critical_factor))
        add_host_scope('check_load', 'System load', 'check_load -w {0:.1f},{0:.1f},{0:.1f} -c {1:.1f},{1:.1f},{1:.1f}'.format(warn, crit), 1)
    for filename, name, desc, cmd, interval in [
        ('check_memory.py', 'check_memory', 'Memory usage', 'check_memory.py -w 85 -c 90', 1),
        ('check_cpu_stats.sh', 'check_cpu_stats', 'CPU statistics', 'check_cpu_stats.sh -w 95,95,60 -c 100,100,100 -i 1 -n 3', 2),
        ('check_uptime.sh', 'check_uptime', 'Uptime', 'check_uptime.sh 30 60', 60),
        ('check_users', 'check_users', 'Logged-in users', 'check_users -w 75 -c 100', 5),
        ('check_open_files.pl', 'check_open_files', 'Open files', 'check_open_files.pl -w 30 -c 50', 5),
        ('check_procs', 'check_all_procs', 'All processes', 'check_procs', 5),
    ]:
        if filename in available:
            if filename in ('check_memory.py', 'check_cpu_stats.sh', 'check_uptime.sh', 'check_open_files.pl'):
                add_host_scope(name, desc, cmd, interval)
            else:
                add(name, desc, cmd, interval, 'system')
    if 'check_swap' in available:
        if swaps:
            add_host_scope('check_swap', 'Free swap space', 'check_swap -w 20% -c 10%', 5)
        else:
            notices.append('Swap: no active swap area; omitted check_swap to avoid irrelevant alarms')
    http_ports = set()
    if 'nginx' in detected or 'apache' in detected:
        http_ports.update({80, 8080, 443, 8443} & ports)
    if 'gunicorn' in detected:
        http_ports.update(gunicorn_bind_ports(detected['gunicorn']) & ports)
    if 'check_http' in available:
        for port in sorted(http_ports):
            tls = ' -S' if port in (443, 8443) else ''
            add('check_http_' + str(port), 'Local HTTP port ' + str(port),
                'check_http -H localhost -I 127.0.0.1 -p ' + str(port) + tls, 2, 'application')
    elif http_ports:
        notices.append('Web server detected but check_http is unavailable; no HTTP response check generated')
    # check_procs is preferred for ordinary services. check_process is a
    # fall-back when it is the only installed process-count plugin. Alpine,
    # Debian and distro-specific plugins can have different interfaces, so
    # generated commands still go through test_configuration().
    generic_process_plugin = ('check_procs' if 'check_procs' in available else
                              'check_process' if 'check_process' in available else None)
    for service, matches in detected.items():
        specialty = [p for p in SERVICES[service][1] if p in available]
        if specialty:
            for plugin in specialty:
                command, reason = specialty_suggestion(service, plugin, ports)
                suggestion_name = 'check_service_' + service + '_' + re.sub(r'[^A-Za-z0-9_]+', '_', plugin)
                suggestion = '[{}] {} service health;{};1;2 {}'.format(
                    suggestion_name, service, command,
                    json.dumps(dict(labels, **{'class': 'application'}), separators=(',', ':')))
                suggestions.append('# Suggested for {}: {}.{}\n# {}'.format(
                    service, reason, ' Observed default port.' if SERVICE_PORTS.get(service) in ports else ' Default port not observed; verify before enabling.', suggestion))
        # Almond executes checks as child processes. check_procs ignores its
        # parent, so `check_procs -C almond` can report zero from within
        # Almond even if it reports one from an interactive shell.
        # check_process -C almond -c 1:5 is the known working alternative.
        if service == 'almond':
            command = 'check_process -C almond -c 1:5'
            metadata = json.dumps(dict(labels, **{'class': 'application'}), separators=(',', ':'))
            if 'check_process' in available:
                add('check_almond', 'Check Almond scheduler', command, 1, 'application')
                notices.append('Almond self-monitoring: using check_process instead of check_procs; validate the result in the running Almond scheduler.')
            else:
                notices.append('Almond self-monitoring: check_process missing; deliberately omitted check_procs -C almond (false CRITICAL from within Almond).')
                suggestions.append('# Suggested: install check_process and verify self-monitoring from Almond before enabling\n# [check_almond] Check Almond scheduler;{};1;1 {}'.format(command, metadata))
            # Keep the separate check_almond_ps API candidate, if available.
            continue
        if generic_process_plugin is None:
            notices.append(service + ': neither check_procs nor check_process is available; no automatic process check generated')
            continue
        for proc_name in sorted({x[0] for x in matches}):
            extra = ''
            if service in ('supervisord', 'gunicorn') and proc_name.startswith('python'):
                extra = ' -a ' + service
            if service in CMD_REQUIRED:
                candidates = [needle for needle in CMD_REQUIRED[service]
                              if any(needle in cmd.lower() for name, cmd in matches if name == proc_name)]
                if not candidates:
                    continue
                needle = candidates[0]
                sample = next(cmd for name, cmd in matches if name == proc_name and needle in cmd.lower())
                index = sample.lower().index(needle)
                extra = ' -a ' + shlex.quote(sample[index:index + len(needle)])
            add('check_proc_' + service + ('_' + proc_name if len({x[0] for x in matches}) > 1 else ''),
                'Process ' + service + ' (' + proc_name + ')',
                generic_process_plugin + ' -C ' + proc_name + extra + ' -c 1:', 1, 'application')
    for proc_name, script in python_apps(procs):
        if 'check_procs' not in available:
            notices.append('Python application detected: ' + script + '; check_procs unavailable')
            continue
        # Stable suffix distinguishes paths that sanitize to the same check name.
        suffix = re.sub(r'[^A-Za-z0-9]+', '_', script.strip('/')).strip('_')[:48]
        digest = hashlib.sha256((proc_name + '\0' + script).encode()).hexdigest()[:8]
        add('check_app_' + suffix + '_' + digest, 'Python application ' + script,
            'check_procs -C ' + proc_name + ' -a ' + shlex.quote(script) + ' -c 1:', 1, 'application')
    # Oracle PMON names contain the instance identifier; discover each separately.
    oracle_instances = set()
    for _comm, cmd in procs:
        for match in re.finditer(r'(?:^|\s)(ora_pmon_[A-Za-z0-9_$#]+)(?=\s|$)', cmd, re.I):
            oracle_instances.add(match.group(1))
    if oracle_instances:
        found_plugins = [name for name in ('check_oracle', 'check_oracle_health', 'check_oracle_health.pl') if name in available]
        notices.append('Oracle instances: {}; matching plugins: {}. Database-level checks require configuration.'.format(
            ', '.join(sorted(oracle_instances)), ', '.join(found_plugins) or 'none'))
        if 'check_procs' in available:
            for instance in sorted(oracle_instances):
                suffix = re.sub(r'[^A-Za-z0-9_]+', '_', instance)
                digest = hashlib.sha256(instance.encode()).hexdigest()[:8]
                add('check_oracle_' + suffix[:40] + '_' + digest, 'Oracle instance ' + suffix,
                    'check_procs -a ' + shlex.quote(instance) + ' -c 1:', 1, 'application')
    # A running systemd unit is useful evidence even without a product-specific rule.
    already_covered = {(name, cmd) for matches in detected.values() for name, cmd in matches}
    for unit in units:
        if unit.get('active') != 'active' or unit.get('sub') != 'running' or not unit.get('pid'):
            continue
        if unit.get('type') == 'oneshot':
            continue
        stem = unit['unit'].removesuffix('.service') if hasattr(str, 'removesuffix') else unit['unit'][:-8]
        stem = stem.split('@', 1)[0].lower().replace('-', '_')
        specific = [name for name in available if re.sub(r'\.(?:py|pl|sh)$', '', name.lower()).replace('-', '_') == 'check_' + stem]
        if specific:
            notices.append('systemd {}: matching plugins {}; verify their arguments and credentials'.format(unit['unit'], ', '.join(sorted(specific))))
        record = process_record(unit['pid'], proc_root)
        cmdline = ' '.join(record['argv'])
        if record['comm'] == 'almond':
            # Never reintroduce a check_procs parent-exclusion false alarm.
            continue
        if (record['comm'], cmdline) in already_covered:
            continue
        # PMON checks above are more specific than the associated generic service.
        if any(instance in cmdline for instance in oracle_instances):
            continue
        selector = selector_for_process(record)
        if not selector or 'check_procs' not in available:
            notices.append('systemd {} is running; a safe process selector or check_procs is unavailable'.format(unit['unit']))
            continue
        clean = re.sub(r'[^A-Za-z0-9_]+', '_', unit['unit']).strip('_')[:45]
        digest = hashlib.sha256(unit['unit'].encode()).hexdigest()[:8]
        add('check_unit_' + clean + '_' + digest, 'Systemd service ' + unit['unit'],
            'check_procs ' + selector + ' -c 1:', 1, 'application')
    # TCP checks are only created when an active systemd service owns the socket.
    active_units = {u['unit'] for u in units if u.get('active') == 'active' and u.get('sub') == 'running'}
    endpoints = set()
    for entry in sockets:
        if entry['protocol'] != 'tcp' or not any(o.get('unit') in active_units for o in entry['owners']):
            continue
        host = '127.0.0.1' if entry['address'] == '0.0.0.0' else entry['address']
        endpoints.add((host, entry['port']))
    if endpoints and 'check_tcp' not in available:
        notices.append('TCP listeners found; check_tcp is unavailable')
    for host, port in sorted(endpoints)[:64]:
        if 'check_tcp' not in available:
            break
        suffix = hashlib.sha256((host + ':' + str(port)).encode()).hexdigest()[:8]
        add('check_tcp_{}_{}'.format(port, suffix), 'Local TCP {} port {}'.format(host, port),
            'check_tcp -H {} -p {}'.format(host, port), 2, 'network')
    if len(endpoints) > 64:
        notices.append('Only 64 TCP endpoints were selected for checks; remaining listeners are inventory only')
    if gateway_suggestion:
        gateway_ip = default_gateway_ipv4() if gateway is None else gateway
        ping_for_gateway = choice or next((p for p in preferred_ping_plugins(platform, ping_preference)
                                           if p in available), None)
        if gateway_ip and ping_for_gateway:
            try:
                gateway_ip = str(ipaddress.IPv4Address(gateway_ip))
                entry = '[check_gateway] Default gateway IPv4;{} -H {} -w 100,20% -c 500,60%;1;2 {}'.format(
                    ping_for_gateway, gateway_ip,
                    json.dumps(dict(labels, **{'class': 'network'}), separators=(',', ':')))
                suggestions.append('# Suggested external network check: verify gateway ICMP policy (never automatically enabled)\n# ' + entry)
            except ipaddress.AddressValueError:
                notices.append('Default gateway detection returned an invalid IPv4 address; no suggestion generated')
    lines = [HEADER, '# Generated by almond_discovery_agent_v5.py; review thresholds and plugin arguments.',
             '# Intervals are in minutes. Service-specific plugin suggestions are listed below.']
    lines += ['# ' + note for note in notices]
    lines += rows
    if suggestions:
        lines += ['', '# Service-specific candidates (disabled until reviewed):'] + suggestions
    return '\n'.join(lines) + '\n', detected, notices


def test_check(plugin_dir, command, timeout):
    """Run exactly the generated argument vector, without a shell, in caller's cwd."""
    try:
        args = shlex.split(command)
        if not args or not SAFE_NAME.fullmatch(args[0]):
            return False, 'INVALID', 'Invalid plugin name'
        args[0] = str((Path(plugin_dir) / args[0]).absolute())
        with tempfile.TemporaryFile() as output:
            proc = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=output,
                                    stderr=subprocess.STDOUT, start_new_session=True)
            try:
                code = proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
                return False, 'TIMEOUT', 'Exceeded {} seconds'.format(timeout)
            output.seek(0)
            data = output.read(65536).decode('utf-8', 'replace')
        lines = [line.strip() for line in data.splitlines() if line.strip()]
        message = lines[-1][:300] if lines else '(no output)'
        if not lines:
            return False, 'INVALID', message
        if re.search(r'(?im)^\s*usage\s*:|(?:invalid|unknown|unrecognized|unrecognised) (?:command.line )?(?:option|argument)', data):
            return False, 'INVALID', message
        if code == 0:
            return True, 'OK', message
        if code == 1:
            return True, 'WARNING', message
        if code == 2:
            return True, 'CRITICAL', message
        if code == 3:
            return False, 'UNKNOWN', message
        return False, 'ERROR', 'Exit {}: {}'.format(code, message)
    except (OSError, ValueError) as exc:
        return False, 'ERROR', str(exc)


def probe_plugin_help(plugin_dir, name, timeout):
    """Probe each generated plugin with -h / --help; never interpret arbitrary text as code."""
    if not SAFE_NAME.fullmatch(name):
        return None, 'unsafe plugin name'
    program = str((Path(plugin_dir) / name).absolute())
    reasons = []
    for flag in ('-h', '--help'):
        try:
            with tempfile.TemporaryFile() as output:
                proc = subprocess.Popen([program, flag], stdin=subprocess.DEVNULL,
                                        stdout=output, stderr=subprocess.STDOUT,
                                        start_new_session=True)
                try:
                    proc.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    proc.wait()
                    reasons.append(flag + ' timed out')
                    continue
                output.seek(0)
                data = output.read(65536).decode('utf-8', 'replace')
        except OSError as exc:
            return None, str(exc)
        data = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', data)
        # Some plugins run a normal check on unknown options. Only accept actual help-like text.
        if re.search(r'(?im)^\s*(?:usage\s*:|options\s*:|(?:-h|--help)(?:[\s,=]|$)|(?:-w|--warning)(?:[\s,=]|$))', data):
            return data, flag
        reasons.append(flag + ' did not return recognizable help')
    return None, '; '.join(reasons)


def help_option_text(data):
    """Extract short option descriptions and indented continuation lines."""
    result = {'warning': [], 'critical': []}
    current = None
    for line in data.splitlines():
        match = re.match(r'^\s*(?:-([wc])\b|--(warning|critical)\b)', line, re.I)
        if match:
            short, longname = match.groups()
            current = 'warning' if (short or '').lower() == 'w' or (longname or '').lower() == 'warning' else 'critical'
            result[current].append(line.strip())
        elif current and line[:1].isspace() and line.strip() and not re.match(r'^\s*-\w', line):
            result[current].append(line.strip())
        else:
            current = None
    return {key: ' '.join(lines) for key, lines in result.items()}


def memory_help_direction(help_text):
    """Infer a direction only when both option descriptions state it explicitly."""
    options = help_option_text(help_text)
    directions = []
    for name in ('warning', 'critical'):
        text = options[name]
        above = bool(re.search(r'\b(?:above|greater than|more than|exceeds?|over)\b', text, re.I))
        below = bool(re.search(r'\b(?:below|less than|under)\b', text, re.I))
        if above == below:
            return None
        directions.append('above' if above else 'below')
    if directions[0] != directions[1]:
        return None
    return directions[0]


def memory_help_conflict(command, help_text):
    """Flag threshold order only when both option descriptions state the same direction."""
    direction = memory_help_direction(help_text)
    if direction is None:
        return None
    try:
        args = shlex.split(command)
        warning = float(args[args.index('-w') + 1])
        critical = float(args[args.index('-c') + 1])
    except (ValueError, IndexError):
        return None
    if direction == 'above' and warning >= critical:
        return 'help says thresholds trigger above values, but warning is not below critical'
    if direction == 'below' and warning <= critical:
        return 'help says thresholds trigger below values, but warning is not above critical'
    return None


def memory_help_defaults(help_text):
    """Read explicit percentage defaults from the two option descriptions."""
    options = help_option_text(help_text)
    result = {}
    for name in ('warning', 'critical'):
        description = options[name]
        if not re.search(r'\bpercent(?:age)?\b|%', description, re.I):
            return None
        match = re.search(
            r'\bdefault(?:\s+value)?(?:\s+(?:is\s+)?(?:set\s+)?(?:to|at))?\s*[:=]?\s*(\d+(?:\.\d+)?)\s*%?',
            description, re.I)
        if not match:
            return None
        value = float(match.group(1))
        if not 0 <= value <= 100:
            return None
        result[name] = value
    if result['warning'] == result['critical']:
        return None
    direction = memory_help_direction(help_text)
    if direction == 'above' and result['warning'] >= result['critical']:
        return None
    if direction == 'below' and result['warning'] <= result['critical']:
        return None
    return result


def apply_memory_help_defaults(command, defaults):
    """Use documented defaults only for the generated memory check."""
    try:
        args = shlex.split(command)
        if not args or args[0] != 'check_memory.py':
            return command
        warn_index, crit_index = args.index('-w') + 1, args.index('-c') + 1
        args[warn_index] = format(defaults['warning'], 'g')
        args[crit_index] = format(defaults['critical'], 'g')
        return ' '.join(shlex.quote(arg) for arg in args)
    except (ValueError, IndexError, KeyError):
        return command


def inspect_plugin_help(conf, plugin_dir, timeout):
    """Inspect each executable once; return adjusted config and argument conflicts."""
    cache = {}
    conflicts = {}
    output = []
    for line in conf.splitlines():
        if not line.startswith('['):
            output.append(line)
            continue
        parts = line.split(';', 3)
        name = parts[0].split(']', 1)[0][1:]
        try:
            argv = shlex.split(parts[1])
        except (ValueError, IndexError):
            output.append(line)
            continue
        if not argv:
            output.append(line)
            continue
        plugin = argv[0]
        if plugin not in cache:
            data, detail = probe_plugin_help(plugin_dir, plugin, timeout)
            cache[plugin] = data
            if data is None:
                print('HELP {}: unavailable ({})'.format(plugin, detail), file=sys.stderr, flush=True)
            else:
                options = help_option_text(data)
                print('HELP {}: obtained with {}'.format(plugin, detail), file=sys.stderr, flush=True)
                for kind in ('warning', 'critical'):
                    if options[kind]:
                        snippet = ''.join(c if c.isprintable() else ' ' for c in options[kind])[:350]
                        print('  {}: {}'.format(kind, snippet), file=sys.stderr, flush=True)
                if not any(options.values()):
                    print('  Threshold meanings not identified; review plugin help manually.', file=sys.stderr, flush=True)
        if plugin == 'check_memory.py' and cache[plugin] is not None:
            direction = memory_help_direction(cache[plugin])
            print('HELP {}: threshold direction {}'.format(
                name, direction if direction else 'not determined; review manually'), file=sys.stderr, flush=True)
            defaults = memory_help_defaults(cache[plugin])
            if defaults:
                parts[1] = apply_memory_help_defaults(parts[1], defaults)
                output.append('# Help: using documented percentage defaults for memory thresholds')
                print('HELP {}: documented defaults warning={:g}% critical={:g}%'.format(
                    name, defaults['warning'], defaults['critical']), file=sys.stderr, flush=True)
            problem = memory_help_conflict(parts[1], cache[plugin])
            if problem:
                conflicts[name] = problem
        output.append(';'.join(parts))
    return '\n'.join(output) + '\n', conflicts


def evaluate_specialty_candidates(conf, plugin_dir, detected, ports, help_timeout, test_timeout,
                                  enable_verified=True):
    """Evaluate commented service candidates; never run unverified arbitrary interfaces.

    Promotion requires a recognized plugin interface, observed endpoint (where
    applicable), and an OK check result. Generic process checks remain as backup.
    """
    available = plugins(plugin_dir)
    help_cache = {}
    output = []
    evaluated = 0
    promoted = 0
    # Only these interface signatures are understood. Other specialty plugins
    # remain documented suggestions, even if their help is available.
    known = {
        'check_almond_ps': ('-H', '-P'),
        'check_http': ('-H', '-I', '-p'),
        'check_pgsql': ('-H', '-p'),
        'check_mysql': ('-H', '-P'),
        'check_redis': ('-H', '-p'),
        'check_ssh': ('-H', '-p'),
    }
    # Avoid auto-enabling probes likely to require credentials or cause lockouts.
    auto_safe = {'check_almond_ps', 'check_http', 'check_ssh'}
    for line in conf.splitlines():
        if not line.startswith('# [check_service_'):
            output.append(line)
            continue
        raw = line[2:]
        try:
            check_name = raw.split(']', 1)[0][1:]
            service = next((x for x in detected if check_name.startswith('check_service_' + x + '_')), None)
            command = raw.split(';', 2)[1]
            argv = shlex.split(command)
            plugin = argv[0]
        except (IndexError, ValueError):
            output.append(line)
            continue
        if service is None or plugin not in available:
            output.append(line)
            continue
        evaluated += 1
        if plugin not in help_cache:
            help_cache[plugin] = probe_plugin_help(plugin_dir, plugin, help_timeout)
        help_text, source = help_cache[plugin]
        reason = None
        if not help_text:
            reason = 'help unavailable: ' + source
        elif plugin not in known:
            reason = 'plugin interface not yet supported by automatic argument inference'
        else:
            required = known[plugin]
            missing = [flag for flag in required if not re.search(
                r'(?<![\w-])' + re.escape(flag) + r'(?=[\s,=]|$)', help_text)]
            if missing:
                reason = 'help does not confirm option(s): ' + ', '.join(missing)
        # Never auto-enable a check against an unobserved endpoint.
        port = SERVICE_PORTS.get(service)
        if not reason and port is not None and port not in ports:
            # HTTP may legitimately use alternate discovered ports.
            actual = None
            for flag in ('-P', '-p'):
                if flag in argv and argv.index(flag) + 1 < len(argv):
                    try:
                        actual = int(argv[argv.index(flag) + 1])
                    except ValueError:
                        pass
            if actual not in ports:
                reason = 'target port not observed listening locally'
        if not reason and plugin not in auto_safe:
            reason = 'requires service-specific credential or safety review'
        if not reason:
            valid, status, message = test_check(plugin_dir, command, test_timeout)
            if not valid or status != 'OK':
                reason = 'probe returned {}: {}'.format(status, message[:120])
        if reason:
            output.append('# Candidate {}: {} (help: {})'.format(check_name, reason, source))
            output.append(line)
            print('CANDIDATE {}: suggested - {}'.format(check_name, reason), file=sys.stderr)
        elif enable_verified:
            output.append('# Candidate {}: verified OK; generic process check retained as fallback'.format(check_name))
            output.append(raw)
            promoted += 1
            print('CANDIDATE {}: enabled after OK probe'.format(check_name), file=sys.stderr)
        else:
            output.append('# Candidate {}: verified OK; automatic activation disabled'.format(check_name))
            output.append(line)
    print('Specialty candidates: evaluated={}, enabled={}'.format(evaluated, promoted), file=sys.stderr)
    return '\n'.join(output) + '\n'


def test_configuration(conf, plugin_dir, timeout, help_conflicts=None):
    """Disable checks that do not produce a usable monitoring result."""
    output = []
    help_conflicts = help_conflicts or {}
    counts = {'OK': 0, 'WARNING': 0, 'CRITICAL': 0, 'DISABLED': 0}
    for line in conf.splitlines():
        if not line.startswith('['):
            output.append(line)
            continue
        parts = line.split(';', 3)
        name = parts[0].split(']', 1)[0][1:]
        valid, status, message = test_check(plugin_dir, parts[1], timeout)
        message = ''.join(c if c.isprintable() else ' ' for c in message)
        print('TEST {}: {} - {}'.format(name, status, message), file=sys.stderr, flush=True)
        if name in help_conflicts:
            print('HELP CONFLICT {}: {}'.format(name, help_conflicts[name]), file=sys.stderr, flush=True)
            valid, status = False, 'HELP_CONFLICT'
        if name == 'check_almond' and (not valid or status != 'OK'):
            # Never auto-activate a failing self-check; do not replace it with
            # check_procs, since Almond is excluded as that plugin's parent.
            counts['DISABLED'] += 1
            output.append('# Test: {} (Almond self-check suggested only)'.format(status))
            output.append('# Suggested: check_process did not return OK in discovery; validate options and test from within Almond')
            output.append('# ' + ';'.join(parts))
            continue
        if not valid:
            parts[2] = '0'
            counts['DISABLED'] += 1
        else:
            counts[status] += 1
        output.append('# Test: {}{}'.format(status, '' if valid else ' (disabled; investigate)'))
        output.append(';'.join(parts))
    return '\n'.join(output) + '\n', counts


def print_inventory(units, systemd_error, sockets, denied, procs, out, platform=None):
    platform = platform if platform is not None else detect_platform()
    print('Environment: ' + platform['detail'], file=out)
    services = detected_services(procs)
    print('Recognized applications: ' + (', '.join(sorted(services)) or 'none'), file=out)
    oracle = sorted({m.group(1) for _comm, cmd in procs for m in
                     re.finditer(r'(?:^|\s)(ora_pmon_[A-Za-z0-9_$#]+)(?=\s|$)', cmd, re.I)})
    if oracle:
        print('Oracle PMON instances: ' + ', '.join(oracle), file=out)
    for _comm, script in python_apps(procs):
        print('Python application: ' + script, file=out)
    for runtime, selector in runtime_candidates(procs):
        print('Runtime candidate ({}): {}'.format(runtime, selector), file=out)
    if systemd_error:
        print('Systemd: ' + systemd_error, file=out)
    else:
        print('Systemd services (running and failed):', file=out)
        for unit in sorted(units, key=lambda u: u['unit']):
            print('  {}: {}/{}{}'.format(unit['unit'], unit['active'], unit['sub'],
                  ' pid={}'.format(unit['pid']) if unit['pid'] else ''), file=out)
    print('Local sockets (non-loopback binding does not prove external reachability):', file=out)
    hints = {1521: 'possible Oracle listener', 5000: 'possible Sybase or another service',
             5432: 'possible PostgreSQL', 9092: 'possible Kafka/Redpanda',
             9000: 'possible MinIO or another service'}
    for item in sockets:
        owners = sorted({(o['unit'] or o['comm'] or 'unknown') for o in item['owners']})
        hint = hints.get(item['port'], '')
        print('  {} {}:{} [{}] owner={}{}'.format(
            item['protocol'], item['address'], item['port'], item['scope'],
            ','.join(owners) or 'unknown', '; hint=' + hint if hint else ''), file=out)
    print('UDP entries are bound sockets and may include clients.', file=out)
    if denied:
        print('Could not inspect file descriptors for {} processes; socket owners may be incomplete.'.format(denied), file=out)
    print('', file=out, flush=True)


def write_config(target, content, force):
    target = Path(target)
    if not target.parent.is_dir():
        raise ValueError('Target directory does not exist: ' + str(target.parent))
    if target.is_symlink():
        raise ValueError('Refusing to replace a symbolic link: ' + str(target))
    # Stage in the same directory; use a hard link for exclusive creation when new.
    fd, temp_name = tempfile.mkstemp(prefix='.almond-discovery-', dir=str(target.parent))
    try:
        os.fchmod(fd, 0o644)
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            f.write(content)
            f.flush()
            os.fsync(f.fileno())
        if target.exists():
            if not force:
                raise ValueError('Target already exists; review output and use --force to replace it with a backup')
            if not target.is_file():
                raise ValueError('Target is not a regular file: ' + str(target))
            stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
            backup = target.with_name(target.name + '.bak-' + stamp)
            backup_fd = os.open(str(backup), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with target.open('rb') as src, os.fdopen(backup_fd, 'wb') as dst:
                shutil.copyfileobj(src, dst)
                dst.flush()
                os.fsync(dst.fileno())
            shutil.copystat(target, backup)
            # Preserve existing permissions and ownership on replacement.
            st = target.stat()
            os.chmod(temp_name, st.st_mode & 0o7777)
            os.chown(temp_name, st.st_uid, st.st_gid)
            os.replace(temp_name, target)
            print('Backup: ' + str(backup), file=sys.stderr)
        else:
            try:
                os.link(temp_name, target)  # fails if another writer created the target
            except FileExistsError:
                raise ValueError('Target was created concurrently; rerun after reviewing it')
    finally:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass


def parse_labels(pairs, environment_name):
    """Read optional labels, keeping the generated `class` label authoritative."""
    result = {}
    if environment_name:
        if len(environment_name) > 200 or any(c in environment_name for c in ('\n', '\r')):
            raise ValueError('Invalid --environment value')
        result['environment'] = environment_name
    for pair in pairs:
        if '=' not in pair:
            raise ValueError('Invalid --label (expected KEY=VALUE): ' + repr(pair))
        key, value = pair.split('=', 1)
        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', key) or not value or key == 'class':
            raise ValueError('Invalid --label key/value (class is reserved): ' + repr(key))
        if '\n' in value or '\r' in value or len(value) > 200:
            raise ValueError('Invalid --label value length/content for ' + key)
        if key in result and result[key] != value:
            raise ValueError('Conflicting --label for ' + key)
        result[key] = value
    return result


def add_inode_checks(conf, plugin_dir, timeout, check_help=True):
    """Add inode checks only if check_disk help explicitly confirms -W and -K.

    Leave a commented plausible check if help was skipped or unavailable.
    All active inode checks pass through the normal test_configuration stage.
    """
    disk_lines = [line for line in conf.splitlines()
                  if line.startswith('[check_disk_') and ';check_disk ' in line]
    if not disk_lines:
        return conf
    help_text = None
    if check_help:
        help_text, _ = probe_plugin_help(plugin_dir, 'check_disk', timeout)
    confirmed = bool(help_text and all(re.search(
        r'(?<![A-Za-z0-9_-])' + re.escape(option) + r'(?=[\s,=]|$)', help_text)
        for option in ('-W', '-K')))
    result = []
    for line in conf.splitlines():
        result.append(line)
        if line not in disk_lines:
            continue
        parts = line.split(';', 3)
        try:
            base_name = parts[0].split(']', 1)[0][1:]
            args = shlex.split(parts[1])
            mount = args[args.index('-p') + 1]
            interval, metadata = parts[3].split(' ', 1)
            inode_name = 'check_inode_' + base_name[len('check_disk_'):]
            entry = '[{}] Inodes {};check_disk -W 10% -K 5% -p {};1;10 {}'.format(
                inode_name, mount, shlex.quote(mount), metadata)
        except (IndexError, ValueError):
            continue
        if len(entry.encode('utf-8')) >= 1023:
            result.append('# Inode check omitted: generated line exceeds 1023 bytes')
        elif confirmed:
            result.append(entry)
        else:
            result.append('# Suggested inode check: verify check_disk -W/-K support and thresholds')
            result.append('# ' + entry)
    return '\n'.join(result) + '\n'


def prepend_discovery_summary(conf, detected, units):
    """Add a compact coverage overview to the generated configuration."""
    lines = conf.splitlines()
    active = disabled = proposed = 0
    for line in lines:
        if line.startswith('['):
            parts = line.split(';', 3)
            if len(parts) >= 3 and parts[2] == '1':
                active += 1
            else:
                disabled += 1
        elif line.startswith('# ['):
            proposed += 1
    failures = sum(1 for u in units if u.get('active') == 'failed' or u.get('sub') == 'failed')
    summary = [
        '# Discovery summary: {} active, {} disabled, {} suggested checks'.format(active, disabled, proposed),
        '# Detected services: {}'.format(', '.join(sorted(detected)) or 'none'),
        '# Failed systemd units observed at discovery: {}'.format(failures),
        '# Verify critical/warning test results before production use; no credentials are inferred.',
    ]
    lines[1:1] = summary
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--plugins-dir', default='/opt/almond/plugins', help='Plugin directory (default: %(default)s)')
    parser.add_argument('--platform', choices=('auto', 'physical', 'vm', 'container'), default='auto',
                        help='Override detected platform for monitoring policy (default: auto)')
    parser.add_argument('--ping-plugin', choices=('auto', 'check_ping', 'check_icmp'), default='auto',
                        help='Preferred local ICMP plugin, with execution-tested fallback (default: auto)')
    parser.add_argument('--no-gateway-suggestion', action='store_true',
                        help='Omit the commented-out default gateway ICMP suggestion')
    parser.add_argument('--container-host-metrics', action='store_true',
                        help='Enable host-scope load/memory/uptime/CPU/swap/open-files checks inside a container')
    parser.add_argument('--output', default='/etc/almond/plugins.conf', help='Target path for --apply (default: %(default)s)')
    parser.add_argument('--apply', action='store_true', help='Write configuration instead of printing it')
    parser.add_argument('--force', action='store_true', help='Replace existing file after making a backup (requires --apply)')
    parser.add_argument('--inventory-only', action='store_true', help='Show systemd services and sockets without executing plugins')
    parser.add_argument('--test-timeout', type=float, default=15, help='Seconds allowed for each plugin test (default: %(default)s)')
    parser.add_argument('--help-timeout', type=float, default=3, help='Seconds allowed per help attempt (default: %(default)s)')
    parser.add_argument('--load-warning-factor', type=float, default=1.5, help='Load warning multiplier of selected CPU basis (default: 1.5)')
    parser.add_argument('--load-critical-factor', type=float, default=2.0, help='Load critical multiplier of selected CPU basis (default: 2.0)')
    parser.add_argument('--load-cpu-basis', choices=('host', 'effective'), default='host',
                        help='CPU basis for host load averages (default: host; effective includes cgroup quota)')
    parser.add_argument('--environment', default='', help='Optional environment label (e.g. demo)')
    parser.add_argument('--label', action='append', default=[], metavar='KEY=VALUE',
                        help='Additional monitoring label; may be repeated (class is reserved)')
    parser.add_argument('--suggest-only', action='store_true', help='Evaluate specialty checks but leave all suggestions commented')
    parser.add_argument('--skip-plugin-help', action='store_true', help='Do not execute plugins with -h or --help')
    args = parser.parse_args()
    if not (math.isfinite(args.load_warning_factor) and math.isfinite(args.load_critical_factor) and 0 < args.load_warning_factor < args.load_critical_factor <= 100):
        parser.error('load factors must be finite, positive, and warning < critical <= 100')
    if args.force and not args.apply:
        parser.error('--force requires --apply')
    if args.inventory_only and args.apply:
        parser.error('--inventory-only cannot be combined with --apply')
    if not 0 < args.test_timeout <= 3600:
        parser.error('--test-timeout must be greater than 0 and no more than 3600')
    if not 0 < args.help_timeout <= 3600:
        parser.error('--help-timeout must be greater than 0 and no more than 3600')
    try:
        platform = platform_profile(args.platform)
        units, systemd_error = systemd_inventory()
        sockets, denied = socket_inventory()
        procs = processes()
        print_inventory(units, systemd_error, sockets, denied, procs,
                        sys.stdout if args.inventory_only else sys.stderr, platform=platform)
        if args.inventory_only:
            return 0
        labels = parse_labels(args.label, args.environment)
        print('Selecting local ICMP plugin as UID {} (the Almond runtime user should match)...'.format(os.geteuid()),
              file=sys.stderr, flush=True)
        network_selection = select_local_ping(args.plugins_dir, platform, args.test_timeout,
                                              preference=args.ping_plugin)
        conf, found, notes = generate(args.plugins_dir, procs=procs, units=units, sockets=sockets,
                                      load_warning_factor=args.load_warning_factor,
                                      load_critical_factor=args.load_critical_factor,
                                      load_cpu_basis=args.load_cpu_basis, labels=labels,
                                      platform=platform, ping_preference=args.ping_plugin,
                                      network_selection=network_selection,
                                      container_host_metrics=args.container_host_metrics,
                                      gateway_suggestion=not args.no_gateway_suggestion)
        if not any(line.startswith('[') for line in conf.splitlines()):
            raise ValueError('No checks could be generated; refusing to write an empty configuration')
        print('Detected services: ' + (', '.join(sorted(found)) or 'none'), file=sys.stderr)
        conflicts = {}
        if not args.skip_plugin_help:
            print('Inspecting plugin help (per-attempt timeout {}s)...'.format(args.help_timeout), file=sys.stderr, flush=True)
            conf, conflicts = inspect_plugin_help(conf, args.plugins_dir, args.help_timeout)
        conf = add_inode_checks(conf, args.plugins_dir, args.help_timeout,
                                check_help=not args.skip_plugin_help)
        print('Testing plugins as user ID {} (per-check timeout {}s)...'.format(os.geteuid(), args.test_timeout), file=sys.stderr, flush=True)
        conf, counts = test_configuration(conf, args.plugins_dir, args.test_timeout, conflicts)
        print('Test summary: ' + ', '.join('{}={}'.format(k, v) for k, v in counts.items()), file=sys.stderr)
        if not args.skip_plugin_help:
            conf = evaluate_specialty_candidates(conf, args.plugins_dir, found, local_listening_ports(), args.help_timeout, args.test_timeout, not args.suggest_only)
        conf = prepend_discovery_summary(conf, found, units)
        print('Review thresholds, test results, and plugin suggestions before using them.', file=sys.stderr)
        if args.apply:
            if sum(counts[k] for k in ('OK', 'WARNING', 'CRITICAL')) == 0:
                sys.stdout.write(conf)
                raise ValueError('No usable checks; refusing to write configuration')
            write_config(args.output, conf, args.force)
            print('Wrote: ' + args.output, file=sys.stderr)
        else:
            sys.stdout.write(conf)
        return 0
    except (OSError, ValueError) as exc:
        print('Error: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
