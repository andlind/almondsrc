from opentelemetry.sdk.metrics import MeterProvider
from opentelemetry.exporter.otlp.proto.http.metric_exporter import OTLPMetricExporter
from opentelemetry.sdk.metrics.export import PeriodicExportingMetricReader
from opentelemetry.sdk.resources import Resource
from opentelemetry.metrics import get_meter_provider, set_meter_provider
from opentelemetry.sdk._logs import LoggerProvider
from opentelemetry.sdk._logs.export import BatchLogRecordProcessor
from opentelemetry.exporter.otlp.proto.http._log_exporter import OTLPLogExporter
from opentelemetry._logs import set_logger_provider
from opentelemetry._logs.severity import SeverityNumber

#OTLP_ENDPOINT = "http://localhost:4318"   # or your custom endpoint
OTLP_METRICS_ENDPOINT = "http://host.docker.internal:4318/v1/metrics"
OTLP_LOGS_ENDPOINT = "http://host.docker.internal:4318/v1/logs"

resource = Resource.create({
    "service.name": "almond-monitoring",
})

exporter = OTLPMetricExporter(
    endpoint=OTLP_METRICS_ENDPOINT,
    timeout=5,
)

#reader = PeriodicExportingMetricReader(
#    exporter,
#    export_interval_millis=1000 )

reader = PeriodicExportingMetricReader(exporter)

provider = MeterProvider(
    resource=resource,
    metric_readers=[reader]
)

set_meter_provider(provider)
meter = get_meter_provider().get_meter("almond")

logger_provider = LoggerProvider(resource=resource)
log_exporter = OTLPLogExporter(endpoint=OTLP_LOGS_ENDPOINT)

logger_provider.add_log_record_processor(
    BatchLogRecordProcessor(log_exporter)
)

set_logger_provider(logger_provider)
otel_logger = logger_provider.get_logger("almond-logs")

class CollectorConnectionError(Exception):
    pass

def set_otlp_endpoint(url):
    global OTLP_METRICS_ENDPOINT, OTLP_LOGS_ENDPOINT
    OTLP_METRICS_ENDPOINT = url.rstrip("/") + "/v1/metrics"  # normalize
    OTLP_LOGS_ENDPOINT = url.rstrip("/") + "/v1/logs"

def clean_attributes(attrs):
    return {k: v for k, v in attrs.items() if v is not None}

def nagios_to_otel_severity(code):
    if code == 0:
        return SeverityNumber.INFO
    if code == 1:
        return SeverityNumber.WARN
    if code == 2:
        return SeverityNumber.ERROR
    return SeverityNumber.ERROR

def export_metrics_via_http(otel_objects):
    payload = {"resourceMetrics": []}

    for obj in otel_objects:
        payload["resourceMetrics"].append({
            "resource": {
                "attributes": [
                    {"key": "plugin.name", "value": {"stringValue": obj["resource"]["plugin.name"]}},
                    {"key": "service.name", "value": {"stringValue": obj["resource"]["service.name"]}}
                ]
            },
            "scopeMetrics": [
                {"metrics": obj["metrics"]}
            ]
        })

    try:
        requests.post(
            f"{OTLP_ENDPOINT}/v1/metrics",
            json=payload,
            timeout=3
        )
    except requests.exceptions.ConnectionError:
        raise CollectorConnectionError(
            f"No OpenTelemetry Collector reachable at {OTLP_ENDPOINT}"
        )


def export_logs_via_http(otel_objects):
    payload = {"resourceLogs": []}

    for obj in otel_objects:
        payload["resourceLogs"].append({
            "resource": {
                "attributes": [
                    {"key": "plugin.name", "value": {"stringValue": obj["resource"]["plugin.name"]}},
                    {"key": "service.name", "value": {"stringValue": obj["resource"]["service.name"]}}
                ]
            },
            "scopeLogs": [
                {"logRecords": obj["logs"]}
            ]
        })

    try:
        requests.post(
            f"{OTLP_ENDPOINT}/v1/logs",
            json=payload,
            timeout=3
        )
    except requests.exceptions.ConnectionError:
        raise CollectorConnectionError(
            f"No OpenTelemetry Collector reachable at {OTLP_ENDPOINT}"
        )

def export_otel_data(otel_objects):
    #global logger
    for obj in otel_objects:
        host = obj["resource"]["host.name"]

        for m in obj["metrics"]:
            metric_name = m["name"]
            unit = m.get("unit", "")

            # GAUGE
            if "gauge" in m:
                datapoint = m["gauge"]["dataPoints"][0]
                value = datapoint["asDouble"]

                counter = meter.create_up_down_counter(
                    name=metric_name,
                    unit=unit,
                    description="Nagios gauge metric"
                )

                attributes = clean_attributes(datapoint["attributes"])
                counter.add(
                    value,
                    attributes={"host.name": host, **attributes}
                )

            # HISTOGRAM
            elif "histogram" in m:
                datapoint = m["histogram"]["dataPoints"][0]
                if "sum" not in datapoint:
                    print (datapoint)
                    print (f"Histogram missing 'sum': {m}")
                    continue
                value = datapoint["sum"]  # correct for histogram

                hist = meter.create_histogram(
                    name=metric_name,
                    unit=unit,
                    description="Nagios histogram metric"
                )

                attributes = clean_attributes(datapoint["attributes"])
                hist.record(
                    value,
                    attributes={"host.name": host, **attributes}
                )

            else:
                # Unknown metric type → log and skip
                logger.warning(f"Unknown metric type for {metric_name}: {m}")
                continue

    for log in obj.get("logs", []):
        status_code = log["attributes"].get("plugin.status_code", None)
        if status_code is None:
            sev = SeverityNumber.INFO
        else:
            sev = nagios_to_otel_severity(status_code)
        otel_logger.emit(
            body=log["body"],
            severity_text=log["severityText"],
            severity_number=sev,
            attributes=log["attributes"],
            timestamp=log["timeUnixNano"]
        )

#def export_otel_data(otel_objects):
    #export_metrics_via_http(otel_objects)
    #export_logs_via_http(otel_objects)
