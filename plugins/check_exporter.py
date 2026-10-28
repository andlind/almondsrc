#!/usr/bin/env python3

import argparse
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path
import math
import yaml
from pathlib import Path
from prometheus_client.parser import text_string_to_metric_families

# Matches standard Prometheus metric samples.
METRIC_PATTERN = re.compile(
    r'^([a-zA-Z_:][a-zA-Z0-9_:]*)'
    r'(\{[^{}]*\})?'
    r'\s+'
    r'([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?'
    r'|[+-]?Inf|NaN)'
    r'(?:\s+-?\d+)?$'
)

def validate_no_overlapping_rules(name, rules):
    enabled_rules = [
        rule for rule in rules
        if rule.get("enabled", True)
    ]

    for i, first in enumerate(enabled_rules):
        labels_a = {
            str(key): str(value)
            for key, value in first.get("labels", {}).items()
        }

        for second in enabled_rules[i + 1:]:
            labels_b = {
                str(key): str(value)
                for key, value in second.get("labels", {}).items()
            }

            # Rules overlap unless at least one shared label
            # has a different value.
            overlap = all(
                labels_a[key] == labels_b[key]
                for key in labels_a.keys() & labels_b.keys()
            )

            if overlap:
                raise ValueError(
                    f"Overlapping enabled rules for '{name}': "
                    f"{labels_a} and {labels_b}"
                )

def format_metric_name(metric):
    name = metric["name"]
    labels = metric.get("labels", {})

    if not labels:
        return name

    # Almond's legacy perfdata parser splits on commas and whitespace; encode
    # labels into the metric name instead of emitting Prometheus label syntax.
    encoded_labels = "_".join(
        f"{key.encode('utf-8').hex()}_{value.encode('utf-8').hex()}"
        for key, value in sorted(labels.items())
    )
    return f"{name}_labels_{encoded_labels}"

def fetch_metrics(url):
    """Fetch metrics from the URL and check the HTTP response."""

    request = urllib.request.Request(
        url,
        headers={"Accept": "text/plain"},
        method="GET",
    )

    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            status = response.status
            content_type = response.headers.get(
                "Content-Type", ""
            )
            body = response.read().decode("utf-8")

    except urllib.error.HTTPError as exc:
        raise RuntimeError(
            f"HTTP request failed: {exc.code} {exc.reason}"
        ) from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise RuntimeError(f"Could not reach URL: {exc}") from exc
    except UnicodeDecodeError as exc:
        raise RuntimeError(
            "Response is not valid UTF-8 text"
        ) from exc

    if not 200 <= status < 300:
        raise RuntimeError(f"Unexpected HTTP status: {status}")

    #print(f"[OK] HTTP status: {status}")
    #print(f"[INFO] Content-Type: {content_type}")

    return body

def parse_metrics(body):
    metrics = []

    for family in text_string_to_metric_families(body):
        for sample in family.samples:
            metrics.append({
                "name": sample.name,
                "labels": dict(sample.labels),
                "value": str(sample.value),
            })

    if not metrics:
        raise ValueError("Endpoint returned no metric samples")

    return metrics

def depreicated_parse_metrics(body):
    """Remove comments and parse metric names and values."""

    metrics = []

    for line_number, line in enumerate(body.splitlines(), start=1):
        line = line.strip()

        # Ignore empty lines and Prometheus comment/metadata lines.
        if not line or line.startswith("#"):
            continue

        match = METRIC_PATTERN.fullmatch(line)

        if not match:
            raise RuntimeError(
                f"Invalid or unsupported metric at line "
                f"{line_number}: {line}"
            )

        name, labels, value = match.groups()

        metrics.append({
            "name": name,
            "labels": labels or "",
            "value": value,
        })

    if not metrics:
        raise RuntimeError(
            "Response contains no valid metric samples"
        )

    return metrics

def load_config(config_path):
    with Path(config_path).open("r", encoding="utf-8") as file:
        config = yaml.safe_load(file) or {}

    if not isinstance(config, dict):
        raise ValueError("Config must be a YAML mapping")

    metrics = config.get("metrics", {})

    if not isinstance(metrics, dict):
        raise ValueError("'metrics' must be a YAML mapping")

    supported_types = {
        "above", "below", "equal", "not_equal",
        "outside", "inside",
    }

    for name, configured_rules in metrics.items():
        if isinstance(configured_rules, dict):
            rules = [configured_rules]
        elif isinstance(configured_rules, list):
            rules = configured_rules
        else:
            raise ValueError(
                f"Config for '{name}' must be a mapping or list of rules"
            )

        for rule in rules:
            if not isinstance(rule, dict):
                raise ValueError(f"Rule for '{name}' must be a mapping")

            if not isinstance(rule.get("enabled", True), bool):
                raise ValueError(f"'enabled' for '{name}' must be boolean")

            labels = rule.get("labels", {})
            if not isinstance(labels, dict):
                raise ValueError(f"'labels' for '{name}' must be a mapping")

            comparison = rule.get("type", "above")
            if comparison not in supported_types:
                raise ValueError(
                    f"Unsupported comparison '{comparison}' for '{name}'"
                )

            thresholds = rule.get("thresholds", {})
            if not isinstance(thresholds, dict):
                raise ValueError(
                    f"'thresholds' for '{name}' must be a mapping"
                )

            unknown_levels = set(thresholds) - {"warning", "critical"}
            if unknown_levels:
                raise ValueError(
                    f"Unknown threshold levels for '{name}': "
                    f"{sorted(unknown_levels)}"
                )

            for level, threshold in thresholds.items():
                if comparison in ("outside", "inside"):
                    if (
                        not isinstance(threshold, dict)
                        or set(threshold) != {"min", "max"}
                    ):
                        raise ValueError(
                            f"Threshold '{level}' for '{name}' "
                            "must contain min and max"
                        )

                    values = [threshold["min"], threshold["max"]]

                    if any(
                        isinstance(value, bool)
                        or not isinstance(value, (int, float))
                        or not math.isfinite(value)
                        for value in values
                    ):
                        raise ValueError(
                            f"Threshold '{level}' for '{name}' "
                            "must have finite numeric bounds"
                        )

                    if values[0] > values[1]:
                        raise ValueError(
                            f"Threshold '{level}' for '{name}': "
                            "min must not exceed max"
                        )
                else:
                    if (
                        isinstance(threshold, bool)
                        or not isinstance(threshold, (int, float))
                        or not math.isfinite(threshold)
                    ):
                        raise ValueError(
                            f"Threshold '{level}' for '{name}' "
                            "must be a finite number"
                        )

        validate_no_overlapping_rules(name, rules)
        metrics[name] = rules

    return metrics

def deprecated_load_config(config_path):
    with Path(config_path).open("r", encoding="utf-8") as file:
        config = yaml.safe_load(file) or {}

    if not isinstance(config, dict):
        raise ValueError("Config must be a YAML mapping")

    metrics = config.get("metrics", {})

    if not isinstance(metrics, dict):
        raise ValueError("'metrics' must be a YAML mapping")

    supported_types = {
        "above", "below", "equal", "not_equal", "outside", "inside"
    }

    def validate_rule(name, rule):
        if not isinstance(rule, dict):
            raise ValueError(f"Rule for '{name}' must be a mapping")

        if not isinstance(rule.get("enabled", True), bool):
            raise ValueError(f"'enabled' for '{name}' must be boolean")

        labels = rule.get("labels", {})
        if not isinstance(labels, dict):
            raise ValueError(f"'labels' for '{name}' must be a mapping")

        comparison = rule.get("type", "above")
        if comparison not in supported_types:
            raise ValueError(
                f"Unsupported comparison type '{comparison}' for '{name}'"
            )

        thresholds = rule.get("thresholds", {})
        if not isinstance(thresholds, dict):
            raise ValueError(f"'thresholds' for '{name}' must be a mapping")

        if set(thresholds) - {"warning", "critical"}:
            raise ValueError(f"Unknown threshold level for '{name}'")

        for level, threshold in thresholds.items():
            if comparison in ("outside", "inside"):
                if not isinstance(threshold, dict) or set(threshold) != {"min", "max"}:
                    raise ValueError(
                        f"'{name}' {level} must contain min and max"
                    )

                bounds = [threshold["min"], threshold["max"]]
                if (
                    any(
                        isinstance(v, bool)
                        or not isinstance(v, (int, float))
                        or not math.isfinite(v)
                        for v in bounds
                    )
                    or bounds[0] > bounds[1]
                ):
                    raise ValueError(f"Invalid range for '{name}' {level}")
            elif (
                isinstance(threshold, bool)
                or not isinstance(threshold, (int, float))
                or not math.isfinite(threshold)
            ):
                raise ValueError(
                    f"Threshold '{level}' for '{name}' must be numeric"
                )

    for name, rules in metrics.items():
        # Keep backward compatibility with a single rule.
        if isinstance(rules, dict):
            rules = [rules]
        elif not isinstance(rules, list):
            raise ValueError(
                f"Config for '{name}' must be a mapping or list of rules"
            )

        for rule in rules:
            validate_rule(name, rule)

        metrics[name] = rules

    return metrics

def evaluate_metric(metric, config):
    name = metric["name"]

    if name not in config:
        return None, None

    try:
        value = float(metric["value"])
    except (ValueError, TypeError):
        return "UNKNOWN", f"{name} has invalid numeric value"

    if not math.isfinite(value):
        return "UNKNOWN", f"{name} has non-finite value"

    #raw_labels = metric.get("labels", "")
    #labels = {}

    #label_pattern = re.compile(
    #    r'([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*"((?:\\.|[^"\\])*)"'
    #)

    #if raw_labels:
    #    for match in label_pattern.finditer(raw_labels.strip("{}")):
    #        key, val = match.groups()
    #        val = (
    #            val.replace(r"\n", "\n")
    #               .replace(r"\"", '"')
    #               .replace(r"\\", "\\")
    #        )
    #        labels[key] = val
    labels = metric.get("labels", {})

    rules = config[name]
    if isinstance(rules, dict):
        rules = [rules]

    severity = {"OK": 0, "WARNING": 1, "CRITICAL": 2}
    best_state = None
    best_problem = None

    for rule in rules:
        if not rule.get("enabled", True):
            continue

        expected = rule.get("labels", {})
        if any(
            labels.get(key) != str(val)
            for key, val in expected.items()
        ):
            continue

        thresholds = rule.get("thresholds", {})
        comparison = rule.get("type", "above")

        def crossed(threshold):
            if comparison == "above":
                return value > threshold
            if comparison == "below":
                return value < threshold
            if comparison == "equal":
                return value == threshold
            if comparison == "not_equal":
                return value != threshold
            if comparison == "outside":
                return (
                    value < threshold["min"]
                    or value > threshold["max"]
                )
            if comparison == "inside":
                return threshold["min"] <= value <= threshold["max"]
            return False

        state = "OK"
        problem = None

        for level in ("critical", "warning"):
            if level not in thresholds:
                continue

            threshold = thresholds[level]
            if crossed(threshold):
                state = level.upper()

                if comparison in ("outside", "inside"):
                    detail = (
                        f"{comparison} {level} range "
                        f"{threshold['min']:g}..{threshold['max']:g}"
                    )
                else:
                    detail = (
                        f"{comparison} {level} threshold {threshold:g}"
                    )

                #label_text = raw_labels
                #problem = f"{name}{label_text} {detail}"
                label_text = ""
                if labels:
                    formatted_labels = ",".join(
                        f'{key}="{value}"'
                        for key, value in sorted(labels.items())
                )
                label_text = f"{{{formatted_labels}}}"
                problem = f"{name}{label_text} {detail}"
                break

        if best_state is None or severity[state] > severity[best_state]:
            best_state = state
            best_problem = problem

    if best_state is None:
        return None, None

    return best_state, best_problem

def main():
    parser = argparse.ArgumentParser(
        description="Check exporter metrics and thresholds"
    )
    parser.add_argument(
        "-H", dest="url", required=True, help="Exporter URL"
    )
    parser.add_argument(
        "-c", dest="config", required=True,
        type=Path, help="Path to config file"
    )
    parser.add_argument(
        "-l",
        "--list-metrics",
        action="store_true",
        help="List all available metric names from the endpoint and exit"
    )

    try:
        args = parser.parse_args()
        
        if args.list_metrics:
            body = fetch_metrics(args.url)
            metrics = parse_metrics(body)

            names = sorted({metric["name"] for metric in metrics})

            print("Available metrics:")
            for name in names:
                print(name)
            return 0
        
        if args.config is None:
            parser.error("-c CONFIG is required unless --list-metrics is used")

        config = load_config(args.config)
        body = fetch_metrics(args.url)
        metrics = parse_metrics(body)
        metrics.append({
            "name": "exporter_healthy",
            "labels": "",
            "value": "1",
        })

        results = []
        perfdata = []
        problems = []

        for metric in metrics:
            state, problem = evaluate_metric(metric, config)

            if state is None:
                continue

            if state == "UNKNOWN":
                raise RuntimeError(problem)

            results.append(state)

            name = metric["name"]
            labels = metric["labels"]
            value = metric["value"]

            #perfdata.append(f"{name}{labels}={value}")
            metric_name = format_metric_name(metric)
            perfdata.append(f"{metric_name}={metric['value']}")

            if problem:
                problems.append(problem)

        processed = len(results)

        # Overall state: CRITICAL > WARNING > OK.
        if "CRITICAL" in results:
            state = "CRITICAL"
            exit_code = 2
        elif "WARNING" in results:
            state = "WARNING"
            exit_code = 1
        else:
            state = "OK"
            exit_code = 0

        message = f"{state}: {processed} metrics processed"

        if state != "OK" and problems:
            message += " " + "; ".join(problems)

        if perfdata:
            message += " | " + "; ".join(perfdata)

        print(message)
        return exit_code

    except SystemExit as exc:
        # Preserve normal --help behavior.
        if exc.code == 0:
            return 0

        print(
            f"UNKNOWN: script_failure invalid arguments",
            file=sys.stdout,
        )
        return 3

    except Exception as exc:
        # Never print a traceback during normal monitoring.
        description = str(exc).replace("\n", " ")
        print(f"UNKNOWN: script_failure {description}")
        return 3


if __name__ == "__main__":
    sys.exit(main())
if __name__ == "__main__":
    main()
