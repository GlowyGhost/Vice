fn main() {
    cc::Build::new()
        .cpp(true)
        .file("src/audio/audio.cpp")
        .include("src/audio")
        .compile("audio");

    cc::Build::new()
        .cpp(true)
        .file("src/performance/performance.cpp")
        .compile("performance");

    println!("cargo:rustc-link-lib=ole32");
    println!("cargo:rustc-link-lib=mfplat");
    println!("cargo:rustc-link-lib=mfreadwrite");
    println!("cargo:rustc-link-lib=mfuuid");
    println!("cargo:rustc-link-lib=wmcodecdspuuid");

    winres::WindowsResource::new()
        .set_icon("icons/icon.ico")
        .compile()
        .unwrap();
}