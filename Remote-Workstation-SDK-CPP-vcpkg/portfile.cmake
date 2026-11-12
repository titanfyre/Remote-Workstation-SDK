vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Remote-Workstation-SDK/releases/download/sdk-v0.1.1/remote-workstation-sdk-sdk-0.1.1.tar.gz"
    FILENAME "remote-workstation-sdk-sdk-0.1.1.tar.gz"
    SHA512 28e8ffc475178b066276a7ccd77359bedfa87f6c61bac5ce732280baa5caf422317c4386cf90740c09bf4ca999cec73a92564163ce6810e0b18963046b9a3d17
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME RemoteWorkstationSDK CONFIG_PATH lib/cmake/RemoteWorkstationSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
