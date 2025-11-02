fn main() {
    cc::Build::new()
        .cpp(true)
        .file("src/audio/audio.cpp")
        .compile("audio");

    #[cfg(target_os = "windows")]
    {
        println!("cargo:rustc-link-lib=ole32");
        println!("cargo:rustc-link-lib=mfplat");
        println!("cargo:rustc-link-lib=mfreadwrite");
        println!("cargo:rustc-link-lib=mfuuid");
        println!("cargo:rustc-link-lib=wmcodecdspuuid");
    }
    #[cfg(target_os = "linux")]
    {
        println!("cargo:rustc-link-lib=asound");
        println!("cargo:rustc-link-lib=pulse");
        println!("cargo:rustc-link-lib=gstreamer-1.0");
        println!("cargo:rustc-link-lib=gstreamer-app-1.0");
    }

    tauri_build::build()
}
