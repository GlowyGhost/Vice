use std::thread;
use tauri::{AppHandle, Manager, WebviewUrl, WebviewWindowBuilder,};

mod files;
mod funcs;
mod audio;

pub fn create_or_show_window(app: &AppHandle) {
    if let Some(window) = app.get_webview_window("Vice") {
        window.show().unwrap();
        window.set_focus().unwrap();
    } else {
        WebviewWindowBuilder::new(
            app,
            "Vice",
            WebviewUrl::App("index.html".into()),
        )
        .title("Vice")
        .inner_size(800.0, 600.0)
        .build()
        .unwrap();
    }
}

pub fn create_window() {
    tauri::Builder::default()
        .setup(|app| {
            files::create_files();

            create_or_show_window(&app.handle());

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            funcs::get_soundboard,
            funcs::flutter_print,
            funcs::get_channels,
            funcs::get_devices,
            funcs::pick_menu_sound,
            funcs::new_sound,
            funcs::new_channel,
            funcs::play_sound,
            funcs::edit_channel,
            funcs::edit_soundboard,
            funcs::get_apps,
            funcs::set_output,
            funcs::get_output,
            funcs::get_outputs,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri app");
}

pub fn run() {
    thread::Builder::new()
        .name("audio-thread".into())
        .spawn(|| audio::start())
        .expect("failed to spawn audio thread");

    let args: Vec<String> = std::env::args().collect();
    if !args.contains(&"--background".to_string()) {
        create_window();
    } else {
        loop {
            thread::sleep(std::time::Duration::from_secs(60));
        }
    }
}