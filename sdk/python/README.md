# glasslink Python host SDK

Pack binary cmds, publish over MQTT, chart recipes. Contract: [`docs/contract/cmd-v1.md`](../../docs/contract/cmd-v1.md).

```bash
# from repo root
tools/.venv/bin/pip install -e sdk/python
python -c "import glasslink; print(glasslink.__version__)"
glasslink inject clear --device cyd1 --color 0xF800
```

```python
import glasslink as gl

with gl.Device("cyd1", host="127.0.0.1", wait_ack=True) as d:
    d.publish(gl.pack_clear(1, 1, 0x0011))
    d.publish(gl.pack_batch(1, 2, gl.bar_graph(8, 40, 100, 60, [0.2, 0.5, 0.9])))
```

Node SDK = backlog (`B-sdk-node`).
