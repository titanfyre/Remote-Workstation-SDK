# Remote-Workstation-SDK Python SDK

`pip install remote-workstation-sdk`

Windows x64 Python3.10+. No driver is installed by this package.

```python
from remote_workstation_sdk import Client
with Client() as client:
    info = client.call("host_status")
```

[Operation schema and API](docs/API.md). Check errors and the product licensing state before device creation.
