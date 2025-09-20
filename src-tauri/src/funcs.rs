use crate::files::{self, SoundboardSFX, Channel};

#[tauri::command]
pub(crate) fn get_soundboard() -> Option<Vec<SoundboardSFX>> {
    files::get_soundboard()
}

#[tauri::command]
pub(crate) fn get_channels() -> Option<Vec<Channel>> {
    files::get_channels()
}

#[tauri::command]
pub(crate) fn flutter_print(text: &str) {
    println!("{}", text)
}