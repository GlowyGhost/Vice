#[tauri::command]
pub(crate) fn greet(name: &str) -> String {
    println!("Hello from Rust!");
    format!("Hello, {}! You've been greeted from Rust!", name)
}