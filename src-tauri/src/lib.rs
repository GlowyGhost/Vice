use tauri::{WebviewUrl, WebviewWindowBuilder};

mod files;
mod funcs;

pub fn run() {
    tauri::Builder::default()
        .setup(|app| {
            files::create_files();

            let args: Vec<String> = std::env::args().collect();
            let is_background = args.contains(&"--background".to_string());

            //Spawn C++ background here
            println!("Spawning C++ background process...");

            if !is_background {
                WebviewWindowBuilder::new(
                    app,
                    "Vice",
                    WebviewUrl::App("index.html".into()),
                )
                .title("Vice")
                .inner_size(800.0, 600.0)
                .build()?;
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![funcs::get_soundboard, funcs::flutter_print, funcs::get_channels])
        .run(tauri::generate_context!())
        .expect("error while running tauri app");
}
