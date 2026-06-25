# Refloat - VESC Package
Refloat is a VESC Package for self-balancing skateboards. It aims to:
- Provide a polished and full-featured user experience
- Maintain a clean and reliable codebase that is easy to extend
- Make it easy for anyone to use Refloat as a base for experimentation
- Standardize interfaces so that package clones can interact with other parts of the ecosystem (3rd party apps, light modules, VESC Express addons etc.) in a compatible way

_**If you're looking for README of the actual package, you can find it [here](package_README.md).**_

## Contributing
Contributions are welcome and appreciated, please refer to [Contributing](CONTRIBUTING.md).

## Building
### Requirements
- `gcc-arm-embedded` version 13 or higher
- `cmake`
- `ninja`
- `vesc_tool`

To build the package, run:
```sh
cmake --workflow --preset package
```

This builds `build/package/artifacts/refloat.vescpkg` and checks the package
payload against the VESC package limits.

To run the host test gate without building the package, use:
```sh
cmake --workflow --preset host
```

To build only the package artifact, use:
```sh
cmake --build --preset package
```

If you don't have `vesc_tool` in your `$PATH` (but you have, for example, a downloaded `vesc_tool` binary), you can specify the `vesc_tool` to use:
```sh
cmake --preset package -DREFLOAT_VESC_TOOL_EXECUTABLE=/path/to/vesc_tool
```
For macOS, the path to VESC Tool when installed using the official installer is as follows:
```sh
cmake --preset package -DREFLOAT_VESC_TOOL_EXECUTABLE="/Applications/VESC Tool.app/Contents/MacOS/VESC Tool"
```

## Documentation
[Development Documentation](doc/index.md)
