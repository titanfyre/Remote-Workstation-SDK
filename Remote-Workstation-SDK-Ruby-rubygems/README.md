# Remote-Workstation-SDK Ruby SDK

`gem install remote-workstation-sdk`

Windows x64 Ruby3.1+.

```ruby
require "remote_workstation_sdk"
RemoteWorkstationSDKSDK::Client.open { |client| p client.call("host_status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
