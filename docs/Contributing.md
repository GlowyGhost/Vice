# Contributing

### If you're planning to contribute, please first check the [Roadmap](Roadmap.md)

## Requirements
To work on this project, you need to have installed [Flutter](https://docs.flutter.dev/get-started/install?_gl=1*h6bu5u*_ga*MTg5MDAyODE1OS4xNzUzMTgwMzIy*_ga_04YGWK0175*czE3NTMzNTExMjYkbzIkZzAkdDE3NTMzNTExMjYkajYwJGwwJGgw), [Rust](https://www.rust-lang.org/learn/get-started), [Tauri](https://v2.tauri.app/) and a C++ compiler, such as [MSVC](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version) and [GCC](https://gcc.gnu.org/).

## Info
### Flutter
Everything for Flutter side of things is in the flutter sub-directory. All the files are in the lib/ sub-directory. To build, run `flutter build web` while being in the flutter sub-directory. This will build the app in build/web sub-directory. This will create loads of files to work, this will be put into `build/web`. Tauri is made to use this directory so no need to copy and paste the files somewhere else.

### Tauri
All the files on the Tauri side is  in the root. `src` has the rust files that you should be editing. In the root of the project run `cargo tauri dev` (Use this for hot-reload) or in src-tauri run `cargo run` (For most use-cases, this is better), this will install and build Tauri dependencies in the project before building the actual project. After building it will run the app for you. This will be located in target.

### C++
The files are automatically compilled by `cc-rs` when running `cargo run`. For reasons, Vice tries to not use external libs in C++. To create a C++ file, put the C++ file in an appropriate spot, and use add this template in `build.rs`
```rust
cc::Build::new()
    .cpp() //If the file is a C++ or C file
    .file() //The path to the C/C++ file
    .compile(); //What rust will classify the file as
```
### **NOTE:**
If you are using a C++ file, make sure to wrap all your functions like this:
```cpp
//Includes

//Util functions

extern "C" {
    //Functions called by Rust
}
```

## Help
### Flutter showing an old version
This could be from a **ServiceWorker** or the server being annoying. You can check by running the Vice and into another browser, go into `http://127.0.0.1:5923`. If it's showing an in-date version in you're browser but not in Vice, delete your ServiceWorker. If that's also out-of-date, check by going into `flutter/build/web`, run `python -m http.server` and open `http://localhost:8000`. If the python server is out of date, check if the code is correctly saved in a different IDE (in Notepad or a similar text-editor).

### C/C++ code not updating
Majority of the time, `cc-rs` doesn't rebuild the C/C++ code every build. `cc-rs` only rebuilds the C++ code when any file of the rust code has been modified. To force a rebuild, add or remove an empty line into the `build.rs` file. When you build again, and **STILL** hasn't updated, check if your code is correctly saved in a different IDE (in Notepad or a similar text-editor).
