fn main() {
    cc::Build::new()
        .cpp(true)
        .file("src/audio/audio.cpp")
        .compile("audio");

        
    #[cfg(target_os = "windows")]
    println!("cargo:rustc-link-lib=ole32");

    tauri_build::build()
}
