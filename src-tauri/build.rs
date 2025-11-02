fn main() {
    let mut binding = cc::Build::new();
    #[cfg(target_os = "linux")]
    let mut audio = binding
        .cpp(true)
        .file("src/audio/audio.cpp");

    #[cfg(target_os = "windows")]
    let audio = binding
        .cpp(true)
        .file("src/audio/audio.cpp");

    #[cfg(target_os = "linux")]
    {
        let gst_flags = pkg_config::Config::new()
            .probe("gstreamer-1.0")
            .expect("Failed to find GStreamer");
        let gst_app_flags = pkg_config::Config::new()
            .probe("gstreamer-app-1.0")
            .expect("Failed to find GStreamer App");
        for path in gst_flags.include_paths.iter() {
            audio.include(path);
        }
        for path in gst_app_flags.include_paths.iter() {
            audio.include(path);
        }
    }

    audio.compile("audio");

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
