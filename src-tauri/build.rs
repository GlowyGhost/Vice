fn main() {
    cc::Build::new()
        .cpp(true)
        .file("src/audio/audio.cpp")
        .compile("audio");

    #[cfg(target_os = "windows")]
    println!("cargo:rustc-link-lib=ole32");
    #[cfg(target_os = "linux")]
    {
        println!("cargo:rustc-link-lib=asound");
        println!("cargo:rustc-link-lib=pulse");
    }

    tauri_build::build()
}
