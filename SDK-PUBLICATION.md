# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | remote-workstation-sdk |
| CPP/conan | published_custom_source_index | [remote-workstation-sdk](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | account_email_confirmed_requires_matching_trusted_publishing_policy | Remote-Workstation-SDK.Native |
| NET/nuget | built_verified_requires_trusted_publishing_policy | RemoteWorkstationSDK.SDK [Source](Remote-Workstation-SDK-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | remote-workstation-sdk [Source](Remote-Workstation-SDK-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [remote-workstation-sdk](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [remote-workstation-sdk](https://crates.io/crates/remote-workstation-sdk) [Source](Remote-Workstation-SDK-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [remote-workstation-sdk](https://www.npmjs.com/package/remote-workstation-sdk) [Source](Remote-Workstation-SDK-JavaScript-npm/) |
| Java/maven | published_install_verified | [remote-workstation-sdk](https://central.sonatype.com/artifact/io.github.titancsa/remote-workstation-sdk/1.0.0) [Source](Remote-Workstation-SDK-Java-maven/) |
| Java/jitpack | published_install_verified | [remote-workstation-sdk](https://jitpack.io/#titanfyre/Remote-Workstation-SDK/java-sdk-v1.0.0) [Source](Remote-Workstation-SDK-Java-maven/) |
| Go/modules | published_install_verified | [remote-workstation-sdk](https://pkg.go.dev/github.com/titanfyre/Remote-Workstation-SDK/Remote-Workstation-SDK-Go-modules@v1.0.0) [Source](Remote-Workstation-SDK-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Remote-Workstation-SDK/Remote-Workstation-SDK-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [RemoteWorkstationSDK](https://www.powershellgallery.com/packages/RemoteWorkstationSDK/1.0.0) [Source](Remote-Workstation-SDK-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | remote_workstation_sdk [Source](Remote-Workstation-SDK-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [remote-workstation-sdk](https://rubygems.org/gems/remote-workstation-sdk) [Source](Remote-Workstation-SDK-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/remote-workstation-sdk](https://packagist.org/packages/titancsa/remote-workstation-sdk) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
