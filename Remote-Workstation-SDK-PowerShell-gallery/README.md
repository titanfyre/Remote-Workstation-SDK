# Remote-Workstation-SDK PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module RemoteWorkstationSDK -Scope CurrentUser`.

```powershell
$client=Connect-RemoteWorkstationSDKSDK
try { Invoke-RemoteWorkstationSDKSDK -Client $client -Operation host_status } finally { Disconnect-RemoteWorkstationSDKSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
