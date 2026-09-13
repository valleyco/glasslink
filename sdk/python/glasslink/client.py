"""MQTT client helpers — publish with optional device ack wait."""

from __future__ import annotations

import json
import threading
import time
from typing import Iterable

from glasslink.protocol import mqtt_client, peek_id_seq, topic_ack, topic_bind_set, topic_cmd


class AckGate:
    """Serialize host pacing on device apply+ack (needed after FLAG_URI HTTP)."""

    def __init__(self, devices: Iterable[str]) -> None:
        self.devices = list(devices)
        self._lock = threading.Lock()
        self._wait: dict[tuple[str, int, int], threading.Event] = {}
        self._body: dict[tuple[str, int, int], dict] = {}

    def attach(self, client) -> None:
        for d in self.devices:
            client.subscribe(topic_ack(d), qos=0)

        def on_message(_c, _u, msg) -> None:
            topic = msg.topic
            try:
                body = json.loads(msg.payload.decode())
            except (UnicodeDecodeError, json.JSONDecodeError, AttributeError):
                return
            try:
                mid = int(body.get("id", -1))
                seq = int(body.get("seq", -1))
            except (TypeError, ValueError):
                return
            for d in self.devices:
                if topic != topic_ack(d):
                    continue
                key = (d, mid, seq)
                with self._lock:
                    ev = self._wait.get(key)
                    if ev is None:
                        return
                    self._body[key] = body
                ev.set()
                return

        client.on_message = on_message
        time.sleep(0.05)

    def publish(
        self,
        client,
        payload: bytes,
        *,
        timeout: float = 5.0,
        label: str = "",
        allow_nack: bool = False,
    ) -> int | None:
        mid, seq = peek_id_seq(payload)
        events: list[tuple[tuple[str, int, int], threading.Event]] = []
        for d in self.devices:
            key = (d, mid, seq)
            ev = threading.Event()
            with self._lock:
                self._wait[key] = ev
                self._body.pop(key, None)
            events.append((key, ev))
            info = client.publish(topic_cmd(d), payload, qos=1)
            info.wait_for_publish(timeout=5)
            if not info.is_published():
                raise TimeoutError(f"MQTT publish timeout → {d}")
        last_rc: int | None = None
        for key, ev in events:
            if not ev.wait(timeout=timeout):
                raise TimeoutError(
                    f"ack timeout {label or 'cmd'} id={mid} seq={seq} device={key[0]}"
                )
            with self._lock:
                body = self._body.pop(key, {})
                self._wait.pop(key, None)
            rc = int(body.get("rc", -1))
            last_rc = rc
            if rc != 0:
                msg = f"device nack {label or 'cmd'} rc={rc} id={mid} seq={seq}"
                if allow_nack:
                    print(f"  warn: {msg}")
                else:
                    raise RuntimeError(msg)
        return last_rc


class Device:
    """Thin multi-device publisher (fire-and-forget or AckGate)."""

    def __init__(
        self,
        device: str | list[str],
        *,
        host: str = "127.0.0.1",
        port: int = 1883,
        client_id: str | None = None,
        wait_ack: bool = False,
    ) -> None:
        if isinstance(device, str):
            self.devices = [device]
        else:
            self.devices = list(device)
        self.host = host
        self.port = port
        self._client_id = client_id or f"glasslink-{self.devices[0]}"
        self._client = None
        self._gate: AckGate | None = AckGate(self.devices) if wait_ack else None

    def connect(self):
        self._client = mqtt_client(self.host, self.port, self._client_id)
        self._client.loop_start()
        if self._gate:
            self._gate.attach(self._client)
        return self

    def close(self) -> None:
        if self._client:
            self._client.loop_stop()
            self._client.disconnect()
            self._client = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, *exc) -> None:
        self.close()

    def publish(
        self, payload: bytes, *, timeout: float = 5.0, label: str = "", allow_nack: bool = False
    ) -> int | None:
        if not self._client:
            raise RuntimeError("not connected")
        if self._gate:
            return self._gate.publish(
                self._client, payload, timeout=timeout, label=label, allow_nack=allow_nack
            )
        return self.publish_fast(payload)

    def publish_fast(self, payload: bytes) -> None:
        """Publish without waiting for device ack (animation / high-rate path)."""
        if not self._client:
            raise RuntimeError("not connected")
        for d in self.devices:
            info = self._client.publish(topic_cmd(d), payload, qos=1)
            info.wait_for_publish(timeout=5)
            if not info.is_published():
                raise TimeoutError(f"MQTT publish timeout → {d}")

    def bind_set(self, slot: int, text: str) -> None:
        if not self._client:
            raise RuntimeError("not connected")
        raw = text.encode("utf-8")
        for d in self.devices:
            info = self._client.publish(topic_bind_set(d, slot), raw, qos=1)
            info.wait_for_publish(timeout=5)
            if not info.is_published():
                raise TimeoutError(f"bind publish timeout → {d}")
