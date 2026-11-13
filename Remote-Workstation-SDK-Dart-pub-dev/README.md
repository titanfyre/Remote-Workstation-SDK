# Remote-Workstation-SDK Dart / Flutter Windows SDK

`dart pub add remote_workstation_sdk` or `flutter pub add remote_workstation_sdk`.

```dart
final client=await Client.open(); // import package:remote_workstation_sdk/remote_workstation_sdk.dart
try { print(client.call("host_status")); } finally { client.close(); }
```

Windows x64 only. Calls are synchronous; use a worker isolate for UI responsiveness. For compiled Dart or Flutter Windows deployment, explicitly copy `lib/native/RemoteWorkstationSDKJsonSDK.dll` beside the executable or pass its absolute libraryPath. This is an FFI package; automatic Flutter DLL bundling is not provided. A full Flutter application has not been compiled in this validation. [API](doc/API.md). Matching driver/license required separately.
